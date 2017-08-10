

#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <system_error>
#include <utility>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#ifdef _WIN32
#include "win.h"
#else

#include <err.h>
#include <arpa/inet.h>
#include <sysexits.h>

#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <afp/finder_info.h>
#include <afp/resource_fork.h>

#include "applefile.h"
#include "mapped_file.h"
#include "defer.h"


#ifndef O_BINARY
#define O_BINARY 0
#endif


std::vector<std::string> unlink_list;


bool _f = false;
bool _m = false;
bool _n = false;
bool _p = false;
bool _s = false;
bool _d = false;
unsigned _v = 0;

int _rv = 0;




void throw_damaged_file() {
	throw std::runtime_error("File is damaged.");
}

void throw_not_apple_double() {
	throw std::runtime_error("Not an Apple Double File");
}

void throw_eof() {
	throw std::runtime_error("Unexpected end of file.");
}


void throw_errno() {
	throw std::system_error(errno, std::generic_category());
}


void throw_errno(const std::string &what) {
	throw std::system_error(errno, std::generic_category(), what);
}

void throw_ec(const std::error_code &ec) {
	throw std::system_error(ec);
}

void throw_ec(const std::error_code &ec, const std::string &what) {
	throw std::system_error(ec, what);
}



/*
 * resource is straight data (cadius, nulib2, etc)
 */
void one_flat_file(const std::string &data, const std::string rsrc) noexcept try {

	struct stat rsrc_st;
	int ok;

	if (_v) fprintf(stdout, "Merging %s & %s\n", rsrc.c_str(), data.c_str());

	ok = stat(data.c_str(), &rsrc_st);
	if (ok < 0) {
		if (errno == ENOENT) {
			if (_n) unlink_list.push_back(rsrc);
			return;
		}
		throw_errno("stat");
	}

	// don't try to do directories.
	if (S_ISDIR(rsrc_st.st_mode)) {
		if (!_p) unlink_list.push_back(rsrc);
		return;
	}

	int fd = open(data.c_str(), O_RDONLY | O_BINARY);
	if (fd < 0) {
		if (errno == ENOENT) {
			if (_n) unlink_list.push_back(rsrc);
			return;
		}
		throw_errno("open");
	}
	defer close_fd([fd]{close(fd); });

	if (stat(rsrc.c_str(), &rsrc_st) < 0) throw_errno("stat");



	std::error_code ec;

	if (rsrc_st.st_size == 0) {
		// truncate any existing resource fork.
		if (!afp::resource_fork::remove(data, ec))
			throw_ec(ec, "resource_fork::remove()");

		if (!_p) unlink_list.push_back(rsrc);
		return;
	}

	mapped_file mf(rsrc, mapped_file::readonly, rsrc_st.st_size);

	afp::resource_fork::write(data, mf.data(), mf.size(), ec);
	if (ec) throw_ec(ec, "resource_fork::write()");

	if (!_p) unlink_list.push_back(rsrc);
} catch (const std::exception &ex) {
	_rv = 1;
	fprintf(stderr, "Merging %s failed: %s\n", rsrc.c_str(), ex.what());
}

void one_file(const std::string &data, const std::string &rsrc) noexcept try {

	struct stat rsrc_st;
	int ok;

	if (_v) fprintf(stdout, "Merging %s & %s\n", rsrc.c_str(), data.c_str());

	ok = stat(data.c_str(), &rsrc_st);
	if (ok < 0) {
		if (errno == ENOENT) {
			if (_n) unlink_list.push_back(rsrc);
			return;			
		}
		throw_errno("stat");
	}

	// don't try to do directories.
	if (S_ISDIR(rsrc_st.st_mode)) {
		if (!_p) unlink_list.push_back(rsrc);
		return;
	}

	int fd = open(data.c_str(), O_RDONLY | O_BINARY);
	if (fd < 0) {
		if (errno == ENOENT) {
			if (_n) unlink_list.push_back(rsrc);
			return;
		}
		throw_errno("open");
	}
	defer close_fd([fd]{close(fd); });

	if (stat(rsrc.c_str(), &rsrc_st) < 0) throw_errno("stat");
	if (rsrc_st.st_size == 0) {
		// mmapping a zero-length file throws EINVAL.
		if (!_p) unlink_list.push_back(rsrc);
		return;
	}

	mapped_file mf(rsrc, mapped_file::readonly, rsrc_st.st_size);


	if (mf.size() < sizeof(ASHeader)) throw_not_apple_double();

	ASHeader header;

	{
		ASHeader *tmp = (ASHeader *)mf.data();

		header.magicNum = ntohl(tmp->magicNum);
		header.versionNum = ntohl(tmp->versionNum);
		header.numEntries = ntohs(tmp->numEntries);
	}

	if (header.magicNum != APPLEDOUBLE_MAGIC)
		throw_not_apple_double();

	// v 2 is a super set of v1. v1 had type 7 for os-specific info, since split into
	// separate entries.
	if (header.versionNum != 0x00010000 && header.versionNum != 0x00020000)
		throw_not_apple_double();


	if (header.numEntries * sizeof(ASEntry) + sizeof(ASHeader) > mf.size()) throw_eof();

	ASEntry *begin = (ASEntry *)(mf.data() + sizeof(ASHeader));
	ASEntry *end = &begin[header.numEntries];

	std::for_each(begin, end, [&mf](const ASEntry &tmp){
		ASEntry e;
		e.entryID = ntohl(tmp.entryID);
		e.entryOffset = ntohl(tmp.entryOffset);
		e.entryLength = ntohl(tmp.entryLength);

		// and check for truncation.
		if (!e.entryLength) return;
		if (e.entryOffset > mf.size()) throw_eof();
		if (e.entryOffset + e.entryLength > mf.size()) throw_eof();

	});

	afp::finder_info fi;
	std::error_code ec;
	bool update_fi = false;
	bool fi_ok = false;

	fi_ok = fi.open(data, afp::finder_info::read_write, ec);

	std::for_each(begin, end, [&](const ASEntry &tmp){

		ASEntry e;
		e.entryID = ntohl(tmp.entryID);
		e.entryOffset = ntohl(tmp.entryOffset);
		e.entryLength = ntohl(tmp.entryLength);


		if (e.entryLength == 0) return;
		switch(e.entryID) {

			#if 0
			/* should not exist for apple double! */
			case AS_DATA: {
				ssize_t ok = write(fd, mf.data() + e.entryOffset, e.entryLength);
				if (ok < 0) throw_errno();
				//if (ok != e.entryLength) return -1;
				break;
			}
			#endif
			case AS_RESOURCE: {
				if (e.entryLength == 0) {
					if (!afp::resource_fork::remove(data, ec))
						throw_ec(ec, "resource_fork::remove()");
				} else {
					afp::resource_fork::write(data, mf.data()+ e.entryOffset, e.entryLength, ec);
					if (ec) throw_ec(ec, "resource_fork::write()");
				}
				break;
			}

			case AS_FINDERINFO: {
				/* Apple now includes xattr w/ finder info */
				if (e.entryLength < 32) {
					fputs("Warning: Invalid Finder Info size.\n", stderr);
					break;
				}
				memcpy(fi.data(), mf.data() + e.entryOffset, 32);
				update_fi = true;
				break;
			}

			case AS_PRODOSINFO: {
				if (e.entryLength != 8) {
					fputs("Warning: Invalid ProDOS Info size.\n", stderr);
					break;
				}
				// fi.set_prodos_file_type(); ??? 
				break;
			}
		}
	});

	if (update_fi) {
		if (!fi.write(ec)) {
			throw_ec(ec, "com.apple.FinderInfo");
		}
	}


	if (!_p) unlink_list.push_back(rsrc);

} catch (const std::exception &ex) {
	_rv = 1;
	fprintf(stderr, "Merging %s failed: %s\n", rsrc.c_str(), ex.what());
}

void unlink_files(std::vector<std::string> &unlink_list) {

	for (const auto &path : unlink_list) {
		if (_v) fprintf(stdout, "Deleting %s\n", path.c_str());
		int ok = unlink(path.c_str());
		if (ok < 0) warn("unlink %s", path.c_str());
	}
	unlink_list.clear();
}


std::string is_raw_resource_fork(const std::string &s) {

	auto l = s.length();
	if (l > 17 && !strncmp("_ResourceFork.bin", s.data() + l - 17, 17))
		return s.substr(0, l - 17);

	if (l > 6 && !strncmp("_rsrc_", s.data() + l - 6, 6))
		return s.substr(0, l - 6);

	return "";
}
void one_dir(std::string dir) noexcept {

	DIR *dirp;
	dirent *dp;

	if (_v >= 2) {
		fprintf(stdout, "Processing %s\n", dir.c_str());
	}

	// check for .AppleDouble folder.

	std::vector<std::string> dir_list;

	if (dir.empty()) return;

	while (!dir.empty() && dir.back() == '/') dir.pop_back();
	dir.push_back('/');

	std::string ad = dir + ".AppleDouble/";

	dirp = opendir(ad.c_str());
	if (dirp) {
		while ( (dp = readdir(dirp)) ) {

			std::string name = dp->d_name;
			if (name == ".DS_Store" && _d) {
				unlink_list.push_back(ad + name);
				continue;
			}

			if (name.front() == '.') continue;

			one_file(dir + name, ad + name);
		}
		closedir(dirp);

		unlink_files(unlink_list);
		if (!_p) {
			// try to delete it...
			if (_v) fprintf(stdout, "Deleting %s\n", ad.c_str());
			int ok = rmdir(ad.c_str());
			if (ok < 0) warn("rmdir %s", ad.c_str());
		}
	}


	dirp = opendir(dir.c_str());
	if (dirp) {
		while ( (dp = readdir(dirp)) ) {

			std::string name = dp->d_name;

			if (_d) {
				if (name == ".DS_Store" || name == "._.DS_Store") {
					unlink_list.push_back(dir + name);
					continue;
				}
			}


			if (name.length() > 2 && name[0] == '.' && name[1] == '_') {

				one_file(dir + name.substr(2), dir + name);
				continue;
			}

			/* _ResourceFork.bin or _rsrc_ raw resource data . */
			std::string tmp = is_raw_resource_fork(name);
			if (!tmp.empty()) {
				one_flat_file(dir + tmp, dir + name);
				continue;
			}

			if (!_f && name[0] != '.') {
				std::string tmp = dir + name;
				#ifdef DT_DIR
				if (dp->d_type == DT_DIR) {
					dir_list.push_back(tmp);
					continue;
				}
				#else
				struct stat st;
				if (stat(tmp.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
					dir_list.push_back(tmp);
					continue;
				}
				#endif
			}



		}
		closedir(dirp);
	} else {
		warn("%s", dir.c_str());
	}

	unlink_files(unlink_list);

	for (const auto &path : dir_list) one_dir(path);

}

void usage() {
	fputs("Usage: dot_clean [-fhmnpsv] directory ...\n", stderr);
	exit(EX_USAGE);
}

void help() {
	fputs(
		"Usage: dot_clean [-fhmnpsv] directory ...\n"
		"\n"
		"    -d Delete .DS_Store files.\n"
		"    -f Disable recursion\n"
		"    -h Display help\n"
		"    -m Always delete apple double files\n"
		"    -n Delete apple double files if there is no matching native file\n"
		"    -p Preserve apple double file.\n"
		"    -s Follow symbolic links.\n"
		"    -v Be verbose\n",
		stdout);

	exit(EX_OK);
}

int main(int argc, char **argv) {

	int c;

	while ((c = getopt(argc, argv, "dfhmnpsvo:")) != -1) {
		switch(c) {
			case 'd': _d = true; break;
			case 'f': _f = true; break;
			case 'h': help(); break;
			case 'm': _m = true; break;
			case 'n': _n = true; break;
			case 'p': _p = true; break;
			case 's': _s = true; break;
			case 'v': _v++; break;
			case 'o': {
				if (strcmp(optarg, "-")) {
					FILE *fp = freopen(optarg, "w", stdout);
					if (!fp) warn("freopen %s", optarg);					
				}
				break;
			}
			case ':':
			case '?':
			default:
				usage();
				break;
		}
	}

	argv += optind;
	argc -= optind;

	if (!argc) usage();

	for (int i = 0; i < argc; ++i) one_dir(argv[i]);

	return _rv;
}

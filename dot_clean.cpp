

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
#include <err.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <arpa/inet.h>

#include <sysexits.h>

#include "applefile.h"
#include "mapped_file.h"
#include "defer.h"


std::vector<std::string> unlink_list;


bool _f = false;
bool _m = false;
bool _n = false;
bool _p = false;
bool _s = false;
bool _v = false;

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
	throw std::system_error(errno, std::system_category());
}


void throw_errno(const std::string &what) {
	throw std::system_error(errno, std::system_category(), what);
}


void one_file(const std::string &data, const std::string &rsrc) noexcept try {

	struct stat rsrc_st;

	if (_v) fprintf(stdout, "Merging %s & %s\n", rsrc.c_str(), data.c_str());

	int fd = open(data.c_str(), O_RDONLY);
	if (fd < 0) {

		if (errno == ENOENT) {
			if (_n) unlink_list.push_back(rsrc);
			return;
		}
		throw_errno();
	}
	defer close_fd([fd]{close(fd); });


	if (stat(rsrc.c_str(), &rsrc_st) < 0) throw_errno();
	if (rsrc_st.st_size == 0) {
		// mmapping a zero-length file throws EINVAL.
		if (!_p) unlink_list.push_back(rsrc);
		return;
	}

	mapped_file mf(rsrc, mapped_file::priv, rsrc_st.st_size);


	if (mf.size() < sizeof(ASHeader)) throw_not_apple_double();

	ASHeader *header = (ASHeader *)mf.data();
	header->magicNum = ntohl(header->magicNum);
	header->versionNum = ntohl(header->versionNum);
	header->numEntries = ntohs(header->numEntries);



	if (header->magicNum != APPLEDOUBLE_MAGIC)
		throw_not_apple_double();

	// v 2 is a super set of v1. v1 had type 7 for os-specific info, since split into
	// separate entries.
	if (header->versionNum != 0x00010000 && header->versionNum != 0x00020000)
		throw_not_apple_double();


	if (header->numEntries * sizeof(ASEntry) + sizeof(ASHeader) > mf.size()) throw_eof();

	ASEntry *begin = (ASEntry *)(mf.data() + sizeof(ASHeader));
	ASEntry *end = &begin[header->numEntries];

	std::for_each(begin, end, [&mf](ASEntry &e){
		e.entryID = ntohl(e.entryID);
		e.entryOffset = ntohl(e.entryOffset);
		e.entryLength = ntohl(e.entryLength);

		// and check for truncation.
		if (!e.entryLength) return;
		if (e.entryOffset > mf.size()) throw_eof();
		if (e.entryOffset + e.entryLength > mf.size()) throw_eof();

	});

	std::for_each(begin, end, [&mf,fd](ASEntry &e){

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
				#ifdef __sun__
				int rfd = openat(fd, "com.apple.ResourceFork", O_XATTR | O_CREAT | O_TRUNC | O_WRONLY, 0666);
				if (rfd < 0) throw_errno("com.apple.ResourceFork");
				defer close_fd([rfd](){ close(rfd); });

				ssize_t ok = write(rfd, mf.data() + e.entryOffset, e.entryLength);
				if (ok < 0) throw_errno("com.apple.ResourceFork");
				//if (ok != e.entryLength) return -1;
				#endif
				break;
			}
			case AS_FINDERINFO: {
				/* Apple now includes xattr w/ finder info */
				if (e.entryLength < 32) {
					fputs("Warning: Invalid Finder Info size.\n", stderr);
					break;
				}
				#ifdef __sun__
				int rfd = openat(fd, "com.apple.FinderInfo", O_XATTR | O_CREAT | O_TRUNC | O_WRONLY, 0666);
				if (rfd < 0) throw_errno("com.apple.ResourceFork");
				defer close_fd([rfd](){ close(rfd); });

				ssize_t ok = write(rfd, mf.data() + e.entryOffset, 32);
				if (ok < 0) throw_errno("com.apple.FinderInfo");
				//if (ok != e.entryLength) return -1;
				#endif
				break;
			}
		}
	});


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

void one_dir(std::string dir) noexcept {

	DIR *dirp;
	dirent *dp;

	// check for .AppleDouble folder.

	std::vector<std::string> dir_list;

	if (dir.empty()) return;

	while (!dir.empty() && dir.back() == '/') dir.pop_back();
	dir.push_back('/');

	std::string ad = dir + ".AppleDouble/";

	dirp = opendir(ad.c_str());
	if (dirp) {
		while ( (dp = readdir(dirp)) ) {

			if (dp->d_name[0] == '.') continue;

			std::string name = dp->d_name;

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
			if (name.length() > 2 && name[0] == '.' && name[1] == '_') {

				one_file(dir + name.substr(2), dir + name);
				continue;
			}

			if (!_f && name[0] != '.') {
				std::string tmp = dir + name;
				#ifdef DT_DIR
				if (dp->d_type == DT_DIR) 
					dir_list.push_back(tmp);
				#else
				struct stat st;
				if (stat(tmp.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
					dir_list.push_back(tmp);
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
		"    -f Disable recursion\n"
		"    -h Display help\n"
		"    -m Always delete apple double files\n"
		"    -n Deleted apple double files if there is no matching native file\n"
		"    -p Preserve apple double file.\n"
		"    -s Follow symbolic links.\n"
		"    -v Be verbose\n",
		stdout);

	exit(EX_OK);
}

int main(int argc, char **argv) {

	int c;

	while ((c = getopt(argc, argv, "fhmnpsv")) != -1) {
		switch(c) {
			case 'f': _f = true; break;
			case 'h': help(); break;
			case 'm': _m = true; break;
			case 'n': _n = true; break;
			case 'p': _p = true; break;
			case 's': _s = true; break;
			case 'v': _v = true; break;
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

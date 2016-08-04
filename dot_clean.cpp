

#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <system_error>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#include <sys/types.h>
#include <dirent.h>

#include <arpa/inet.h>

#include <sysexits.h>

#include "applefile.h"
#include "mapped_file.h"


std::vector<std::string> unlink_list;
std::vector<std::string> rmdir_list;


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


class defer {
public:
	typedef std::function<void()> FX;
	defer() = default;

	defer(FX fx) : _fx(fx) {}
	defer(const defer &) = delete;
	defer(defer &&) = default;
	defer & operator=(const defer &) = delete;
	defer & operator=(defer &&) = default;

	void cancel() { _fx = nullptr;  }
	~defer() { if (_fx) _fx(); }
private:
	FX _fx;
};



void one_file(const std::string &data, const std::string &rsrc) noexcept try {

	if (_v) fprintf(stdout, "%s\n", data.c_str());

	int fd = open(data.c_str(), O_RDONLY);
	if (fd < 0) {

		if (errno == ENOENT && _n) { unlink_list.push_back(rsrc); return; }
		throw_errno();
	}
	defer close_fd([fd]{close(fd); });

	mapped_file mf(rsrc, mapped_file::priv);


	if (mf.size() < sizeof(ASHeader)) throw_not_apple_double();

	ASHeader *header = (ASHeader *)mf.data();
	header->magicNum = ntohl(header->magicNum);
	header->versionNum = ntohl(header->versionNum);
	header->numEntries = ntohs(header->numEntries);



	if (header->magicNum != APPLEDOUBLE_CIGAM || header->versionNum != 0x00020000)
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


	for (auto iter = begin; iter != end; ++iter) {
		const auto &e = *iter;

		if (e.entryLength == 0) continue;
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
				if (rfds < 0) throw_errno("com.apple.ResourceFork");
				defer close_fd([rfd](){ close(rfd); });

				ssize_t ok = write(rfd, mf.data() + e.entryOffset, e.entryLength);
				if (ok < 0) throw_errno("com.apple.ResourceFork");
				//if (ok != e.entryLength) return -1;
				#endif
				break;
			}
			case AS_FINDERINFO: {
				if (e.entryLength != 32) {
					fputs("Warning: Invalid Finder Info size.\n", stderr);
					break;
				}
				#ifdef __sun__
				int rfd = openat(fd, "com.apple.FinderInfo", O_XATTR | O_CREAT | O_TRUNC | O_WRONLY, 0666);
				if (rfd < 0) throw_errno("com.apple.ResourceFork");
				defer close_fd([rfd](){ close(rfd); });

				ssize_t ok = write(rfd, mf.data() + e.entryOffset, e.entryLength);
				if (ok < 0) throw_errno("com.apple.FinderInfo");
				//if (ok != e.entryLength) return -1;
				#endif
				break;
			}
		}

	}




	if (!_p) unlink_list.push_back(rsrc);

} catch (std::exception &ex) {
	_rv = 1;
	fprintf(stderr, "%s : %s\n", data.c_str(), ex.what());
}

void one_dir(std::string dir) noexcept {

	DIR *dirp;
	dirent *dp;

	// check for .AppleDouble folder.


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
		if (!_p) rmdir_list.push_back(ad); // delete it if empty.
		closedir(dirp);
	}


	dirp = opendir(dir.c_str());

	while ( (dp = readdir(dirp)) ) {
		if (dp->d_name[0] != '.') continue;
		if (dp->d_name[1] != '_') continue;

		std::string name = dp->d_name;
		one_file(dir + name.substr(2), dir + name);
	}
	closedir(dirp);

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
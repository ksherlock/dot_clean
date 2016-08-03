
#include <string>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <system_error>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#include <sys/types.h>

#include <arpa/inet.h>

#include <sysexits.h>

#include "applefile.h"
#include "mapped_file.h"


void damaged_file() {
	throw std::runtime_error("File is damaged.");
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

void unfork(const char *in, const char *out) {


	static_assert(sizeof(ASHeader) == 26, "ASHeader size is wrong.");
	static_assert(sizeof(ASEntry) == 12, "ASEntry size is wrong.");


	mapped_file mf(in, mapped_file::priv);

	if (mf.size() < sizeof(ASHeader)) damaged_file();

	ASHeader *header = (ASHeader *)mf.data();
	header->magicNum = ntohl(header->magicNum);
	header->versionNum = ntohl(header->versionNum);
	header->numEntries = ntohs(header->numEntries);



	if (header->magicNum != 0x00051600 || header->versionNum != 0x00020000 || header->numEntries == 0)
		throw std::runtime_error("Not an AppleSingle File");

	if (header->numEntries * sizeof(ASEntry) + sizeof(ASHeader) > mf.size()) damaged_file();

	ASEntry *begin = (ASEntry *)(mf.data() + sizeof(ASHeader));
	ASEntry *end = &begin[header->numEntries];

	std::for_each(begin, end, [](ASEntry &e){
		e.entryID = ntohl(e.entryID);
		e.entryOffset = ntohl(e.entryOffset);
		e.entryLength = ntohl(e.entryLength);
	});


	// check for truncation....

	for (auto iter = begin; iter != end; ++iter) {
		const auto &e = *iter;
		if (!e.entryLength) continue;
		if (e.entryOffset > mf.size()) damaged_file();
		if (e.entryOffset + e.entryLength > mf.size()) damaged_file();
	}

	std::string outname = out ? out : "";
	// if no name, pull it from the name record.
	if (!out) {
		auto iter = std::find_if(begin, end, [](const ASEntry &e) { return e.entryID == AS_REALNAME; });
		if (iter != end) {
			outname.assign((const char *)mf.data() + iter->entryOffset, iter->entryLength);
		}
	}

	if (outname.empty()) throw std::runtime_error("No filename");

	int fd = open(outname.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (fd < 1) throw_errno();
	defer close_fd([fd](){ close(fd); });

	for (auto iter = begin; iter != end; ++iter) {
		const auto &e = *iter;

		if (e.entryLength == 0) continue;
		switch(e.entryID) {

			case AS_DATA: {
				ssize_t ok = write(fd, mf.data() + e.entryOffset, e.entryLength);
				if (ok < 0) throw_errno();
				//if (ok != e.entryLength) return -1;
				break;
			}
			case AS_RESOURCE: {
				int fd = openat(fd, "com.apple.ResourceFork", O_XATTR | O_CREAT | O_TRUNC | O_WRONLY, 0666);
				if (fd < 0) throw_errno("com.apple.ResourceFork");
				defer close_fd([fd](){ close(fd); });

				ssize_t ok = write(fd, mf.data() + e.entryOffset, e.entryLength);
				if (ok < 0) throw_errno("com.apple.ResourceFork");
				//if (ok != e.entryLength) return -1;
				break;
			}
			case AS_FINDERINFO: {
				if (e.entryLength != 32) {
					fputs("Warning: Invalid Finder Info size.\n", stderr);
					break;
				}
				int fd = openat(fd, "com.apple.FinderInfo", O_XATTR | O_CREAT | O_TRUNC | O_WRONLY, 0666);
				if (fd < 0) throw_errno("com.apple.ResourceFork");
				defer close_fd([fd](){ close(fd); });

				ssize_t ok = write(fd, mf.data() + e.entryOffset, e.entryLength);
				if (ok < 0) throw_errno("com.apple.FinderInfo");
				//if (ok != e.entryLength) return -1;
				break;
			}
		}

	}

	close(fd);
}

void usage() {
	fputs("Usage: applesingle [-o file] file...\n", stderr);
	exit(EX_USAGE);
}
int main(int argc, char **argv) {


	int c;
	char *_o;

	while ((c = getopt(argc, argv, "o:")) != -1) {

		switch(c) {
			case 'o':
				_o = optarg;
				break;
			case ':':
			case '?':
			default:
				usage();
				break;
		}

	}

	argv += optind;
	argc -= optind;

	int rv = 0;
	for (int i = 0; i < argc; ++i) {

		try { unfork(argv[i], _o); }
		catch (std::exception &ex) {
			fprintf(stderr, "%s : %s\n", argv[i], ex.what());
			rv = 1;
		}
		_o = nullptr;
	}

	return rv;
}
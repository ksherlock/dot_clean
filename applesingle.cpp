/*
 * convert a file to an applesingle file.
 *
 *
 */

#include <system_error>
#include <string>
 #include <vector>

#include <unistd.h>
#include <sysexits.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>

#include "mapped_file.h"
#include "xattr.h"
#include "finder_info_helper.h"

#include "applefile.h"


#if defined(__linux__)
#define XATTR_RESOURCEFORK_NAME "user.com.apple.ResourceFork"
#endif


#if defined (_WIN32)
#define XATTR_FINDERINFO_NAME "AFP_Resource"
#endif

#ifndef XATTR_FINDERINFO_NAME
#define XATTR_FINDERINFO_NAME "com.apple.FinderInfo"
#endif

#ifndef XATTR_RESOURCEFORK_NAME
#define XATTR_RESOURCEFORK_NAME "com.apple.ResourceFork"
#endif


void usage() {
	fputs("Usage: applesingle [-hv] [-o outfile] file ...\n", stderr);
	exit(EX_USAGE);
}

void help() {
	fputs(
		"Usage: applesingle [-hv] [-o outfile] file ...\n"
		"\n"
		"    -o Specify outfile name\n"
		"    -h Display help\n"
		"    -v Be verbose\n",
		stdout);

	exit(EX_OK);
}

bool _v = false;
int _rv = 0;


std::vector<uint8_t> read_resource_fork(const std::string &path, std::error_code ec) {
	std::vector<uint8_t> rv;

	ec.clear();

	#if defined(__sun__) || defined(_WIN32)
	int fd;
	struct stat st;

	#if defined(__sun__)
	fd = attropen(path.c_str(), XATTR_RESOURCEFORK_NAME, O_RDONLY);
	#else
	std::string p(path);
	p += ":" XATTR_RESOURCEFORK_NAME;

	fd = open(p.c_str(), O_RDONLY);
	#endif
	if (fd < 0) {
		ec = std::error_code(errno, std::generic_category());
		return rv;
	}

	if (fstat(fd, &st) < 0) {
		ec = std::error_code(errno, std::generic_category());
		close(fd);
		return rv;
	}

	if (st.st_size == 0) {
		close(fd);
		return rv;
	}

	for(;;) {
		rv.resize(st.st_size);
		ssize_t ok = read(fd, rv.data(), st.st_size);
		if (ok < 0) {
			if (errno == EINTR) continue;
			ec = std::error_code(errno, std::generic_category());
			rv.clear();
			break;
		}
		rv.resize(ok);
		break;
	}
	close fd;
	return rv;
	#else

	int fd = open(path.c_str(), O_RDONLY);
	if (fd < 0) {
		ec = std::error_code(errno, std::generic_category());
		return rv;
	}

	for(;;) {
		ssize_t size = size_xattr(fd, XATTR_RESOURCEFORK_NAME);
		if (size < 0) {
			if (errno == EINTR) continue;
			if (errno != ENOATTR)
				ec = std::error_code(errno, std::generic_category());
			close(fd);
			return rv;
		}
		if (size == 0) break;
		rv.resize(size);

		ssize_t rsize = read_xattr(fd, XATTR_RESOURCEFORK_NAME, rv.data(), size);
		if (rsize < 0) {
			if (errno == ERANGE || errno == EINTR) continue; // try again.
			ec = std::error_code(errno, std::generic_category());
			rv.clear();
			break;
		}
		rv.resize(rsize);
		break;
	}
	close(fd);
	return rv;

	#endif
}


/* check if a file is apple single or apple double format (or neither). */
uint32_t classify(const mapped_file &mf) {
	if (mf.size() < sizeof(ASHeader)) return 0;

	const ASHeader *header = (const ASHeader *)mf.data();
	if (header->magicNum == htonl(APPLESINGLE_MAGIC)) return APPLESINGLE_MAGIC;
	if (header->magicNum == htonl(APPLEDOUBLE_MAGIC)) return APPLEDOUBLE_MAGIC;

	return 0;
}

/*
 * cases to consider
 * 1. single file w/ fork/finder info
 * 2. single file + appledouble file
 * 3. single file w/o fork/finder info
 * 4. file is already apple single / apple double format.
 */

void one_file(const std::string &infile, const std::string &outfile) {

	// 1. check if apple single/apple double
	mapped_file mf;
	ASHeader head;
	ASEntry e;


	try {
		mf = mapped_file(infile);
	} catch(std::exception &ex) {
		warnx("%s: %s", infile.c_str(), ex.what());
		_rv = 1;
		return;
	}

	uint32_t fmt = classify(mf);
	if (fmt == APPLESINGLE_MAGIC) {
		warnx("%s: File is apple single format.", infile.c_str());
		return;
	}


	if (fmt == APPLEDOUBLE_MAGIC) {
		warnx("%s: File is apple double format.", infile.c_str());
		return;
	}


	// flag for preferred data source?
	// 1. check for native fork

	// 2. check for apple double data.

	int count = 2; // name + data fork

	std::error_code ec;

	finder_info_helper fi;
	bool fi_ok = fi.open(infile);


	// ENOATTR is ok... but that's not an errc...
	std::vector<uint8_t> resource = read_resource_fork(infile, ec);

	if (ec && ec.value() != ENOATTR) {
		warnc(ec.value(), "%s resource fork\n", infile.c_str());
		return;
	}

	if (!fi_ok && resource.empty()) {
		warnx("%s: File is not extended.", infile.c_str());
		return;
	}

	if (resource.size()) count++;
	if (fi_ok) count++;


	int fd = open(outfile.c_str(), O_WRONLY | O_CREAT, 0666);
	memset(&head, 0, sizeof(head));
	head.magicNum = htonl(APPLESINGLE_MAGIC);
	head.versionNum = htonl(0x00020000);
	head.numEntries =  htons(count);
	write(fd, &head, sizeof(head));

	off_t offset = sizeof(ASHeader) + sizeof(ASEntry) * count;



	// 1 - name
	e.entryID = htonl(AS_REALNAME);
	e.entryOffset = htonl(offset);
	e.entryLength = htonl(infile.length()); // todo -- basename it!
	write(fd, &e, sizeof(e));

	offset += infile.length();

#if 0
	// 2 - dates 
	e.entryID = htonl(AS_FILEDATES);
	e.entryOffset = htonl(offset);
	e.entryLength = htonl(sizeof(ASFileDates)); 
	write(fd, &e, sizeof(e));

	offset += sizeof(ASFileDates);
#endif

	// 3 - finder info
	if (fi_ok) {
		e.entryID = htonl(AS_FINDERINFO);
		e.entryOffset = htonl(offset);
		e.entryLength = htonl(32);
		write(fd, &e, sizeof(e));

		offset += 32;
	}
	// 4 - data fork.

	e.entryID = htonl(AS_DATA);
	e.entryOffset = htonl(offset);
	e.entryLength = htonl(mf.size());
	write(fd, &e, sizeof(e));

	offset += mf.size();

	// 5 - resource fork?
	if (resource.size()) {

		e.entryID = htonl(AS_RESOURCE);
		e.entryOffset = htonl(offset);
		e.entryLength = htonl(resource.size());
		write(fd, &e, sizeof(e));

		offset += resource.size();
	}

	// now write it..

	// 1 - name
	write(fd, infile.c_str(), infile.length());

	// 2 - dates...

	// 3 - finder info
	if (fi_ok) {
		write(fd, fi.finder_info(), 32);
	}

	// 4 - data fork
	write(fd, mf.data(), mf.size());

	// 5 - resource fork?
	if (resource.size()) {
		write(fd, resource.data(), resource.size());
	}
	close(fd);
}

int main(int argc, char **argv) {

	std::string _o;

	int c;

	while ((c = getopt(argc, argv, "o:v")) != -1) {

		switch(c) {
			case 'v': _v = true; break;
			case 'h': help(); break;
			case 'o': _o = optarg; break;
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

	for (int i = 0; i < argc; ++i) {
		std::string s(argv[i]);
		if (_o.empty()) _o = s + ".applesingle";
		one_file(s, _o);
		_o.clear();
	}
	return _rv;
}
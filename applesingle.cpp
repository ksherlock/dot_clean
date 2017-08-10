/*
 * convert a file to an applesingle file.
 *
 *
 */

#include <system_error>
#include <string>
#include <vector>
#include <memory>

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef _WIN32
#include "win.h"
#else
#include <err.h>
#include <sysexits.h>
#endif

#include "mapped_file.h"

#include <afp/finder_info.h>
#include <afp/resource_fork.h>

#include "applefile.h"

#ifndef O_BINARY
#define O_BINARY 0
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

	afp::finder_info fi;
	afp::resource_fork rf;

	size_t rfork_size = 0;
	std::unique_ptr<uint8_t[]> rfork_data;

	bool fi_ok = fi.open(infile, ec);

	bool rf_ok = rf.open(infile, afp::resource_fork::read_only, ec);

	if (rf_ok) {
		rfork_size = rf.size(ec);
		if (rfork_size) {

			rfork_data.reset(new uint8_t[rfork_size]);
			rfork_size = rf.read(rfork_data.get(), rfork_size, ec);

			if (ec) {
				warnx("%s resource fork: %s", infile.c_str(), ec.message().c_str());
				return;
			}
		}
		rf.close();
	}
	if (!rfork_size) rf_ok = false;


	if (!fi_ok && !rf_ok) {
		warnx("%s: File is not extended.", infile.c_str());
		return;
	}

	if (rf_ok) count++;
	if (fi_ok) count++;


	int fd = open(outfile.c_str(), O_WRONLY | O_CREAT | O_BINARY, 0666);
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

	// 5 - resource fork
	if (rf_ok) {

		e.entryID = htonl(AS_RESOURCE);
		e.entryOffset = htonl(offset);
		e.entryLength = htonl(rfork_size);
		write(fd, &e, sizeof(e));

		offset += rfork_size;
	}

	// now write it..

	// 1 - name
	write(fd, infile.c_str(), infile.length());

	// 2 - dates...

	// 3 - finder info
	if (fi_ok) {
		write(fd, fi.data(), 32);
	}

	// 4 - data fork
	write(fd, mf.data(), mf.size());

	// 5 - resource fork?
	if (rf_ok) {
		write(fd, rfork_data.get(), rfork_size);
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


#include "finder_info_helper.h"

#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <string>

#include <unistd.h>
#include <fcntl.h>

#if defined(__APPLE__)
#include <sys/xattr.h>
#endif

#if defined(__linux__)
#include <sys/xattr.h>
#define XATTR_FINDERINFO_NAME "user.com.apple.FinderInfo"
#define XATTR_RESOURCEFORK_NAME "user.com.apple.ResourceFork"
#endif

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/extattr.h>
#endif

#if defined(_AIX)
#include <sys/ea.h>
#endif


#if defined (_WIN32)
#define XATTR_FINDERINFO_NAME "AFP_AfpInfo"
#endif

#ifndef XATTR_FINDERINFO_NAME
#define XATTR_FINDERINFO_NAME "com.apple.FinderInfo"
#endif


#if defined(_WIN32)
#define _prodos_file_type _afp.prodos_file_type
#define _prodos_aux_type _afp.prodos_file_type
#define _finder_info _afp.finder_info
#endif

namespace {

	/*

     tech note PT515
     ProDOS -> Macintosh conversion

     ProDOS             Macintosh
     Filetype    Auxtype    Creator    Filetype
     $00          $0000     'pdos'     'BINA'
     $B0 (SRC)    (any)     'pdos'     'TEXT'
     $04 (TXT)    $0000     'pdos'     'TEXT'
     $FF (SYS)    (any)     'pdos'     'PSYS'
     $B3 (S16)    (any)     'pdos'     'PS16'
     $uv          $wxyz     'pdos'     'p' $uv $wx $yz

     Programmer's Reference for System 6.0:

     ProDOS Macintosh
     File Type Auxiliary Type Creator Type File Type
     $00        $0000           “pdos”  “BINA”
     $04 (TXT)  $0000           “pdos”  “TEXT”
     $FF (SYS)  (any)           “pdos”  “PSYS”
     $B3 (S16)  $DByz           “pdos”  “p” $B3 $DB $yz
     $B3 (S16)  (any)           “pdos”  “PS16”
     $D7        $0000           “pdos”  “MIDI”
     $D8        $0000           “pdos”  “AIFF”
     $D8        $0001           “pdos”  “AIFC”
     $E0        $0005           “dCpy”  “dImg”
     $FF (SYS)  (any)           “pdos”  “PSYS”
     $uv        $wxyz           “pdos”  “p” $uv $wx $yz


	  mpw standard:
     $uv        (any)          "pdos"  printf("%02x  ",$uv)

     */



	int hex(uint8_t c)
	{
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return c + 10 - 'a';
		if (c >= 'A' && c <= 'F') return c + 10 - 'A';
		return 0;
	}


	bool finder_info_to_filetype(const uint8_t *buffer, uint16_t *file_type, uint32_t *aux_type) {

		if (!memcmp("pdos", buffer + 4, 4))
		{
			if (buffer[0] == 'p') {
				*file_type = buffer[1];
				*aux_type = (buffer[2] << 8) | buffer[3];
				return true;
			}
			if (!memcmp("PSYS", buffer, 4)) {
				*file_type = 0xff;
				*aux_type = 0x0000;
				return true;
			}
			if (!memcmp("PS16", buffer, 4)) {
				*file_type = 0xb3;
				*aux_type = 0x0000;
				return true;
			}

			// old mpw method for encoding.
			if (!isxdigit(buffer[0]) && isxdigit(buffer[1]) && buffer[2] == ' ' && buffer[3] == ' ')
			{
				*file_type = (hex(buffer[0]) << 8) | hex(buffer[1]);
				*aux_type = 0;
				return true;
			}
		}
		if (!memcmp("TEXT", buffer, 4)) {
			*file_type = 0x04;
			*aux_type = 0x0000;
			return true;
		}
		if (!memcmp("BINA", buffer, 4)) {
			*file_type = 0x00;
			*aux_type = 0x0000;
			return true;
		}
		if (!memcmp("dImgdCpy", buffer, 8)) {
			*file_type = 0xe0;
			*aux_type = 0x0005;
			return true;
		}

		if (!memcmp("MIDI", buffer, 4)) {
			*file_type = 0xd7;
			*aux_type = 0x0000;
			return true;
		}

		if (!memcmp("AIFF", buffer, 4)) {
			*file_type = 0xd8;
			*aux_type = 0x0000;
			return true;
		}

		if (!memcmp("AIFC", buffer, 4)) {
			*file_type = 0xd8;
			*aux_type = 0x0001;
			return true;
		}

		return false;
	}

	bool file_type_to_finder_info(uint8_t *buffer, uint16_t file_type, uint32_t aux_type) {
		if (file_type > 0xff || aux_type > 0xffff) return false;

		if (!file_type && aux_type == 0x0000) {
			memcpy(buffer, "BINApdos", 8);
			return true;
		}

		if (file_type == 0x04 && aux_type == 0x0000) {
			memcpy(buffer, "TEXTpdos", 8);
			return true;
		}

		if (file_type == 0xff && aux_type == 0x0000) {
			memcpy(buffer, "PSYSpdos", 8);
			return true;
		}

		if (file_type == 0xb3 && aux_type == 0x0000) {
			memcpy(buffer, "PS16pdos", 8);
			return true;
		}

		if (file_type == 0xd7 && aux_type == 0x0000) {
			memcpy(buffer, "MIDIpdos", 8);
			return true;
		}
		if (file_type == 0xd8 && aux_type == 0x0000) {
			memcpy(buffer, "AIFFpdos", 8);
			return true;
		}
		if (file_type == 0xd8 && aux_type == 0x0001) {
			memcpy(buffer, "AIFCpdos", 8);
			return true;
		}
		if (file_type == 0xe0 && aux_type == 0x0005) {
			memcpy(buffer, "dImgdCpy", 8);
			return true;
		}


		memcpy(buffer, "p   pdos", 8);
		buffer[1] = (file_type) & 0xff;
		buffer[2] = (aux_type >> 8) & 0xff;
		buffer[3] = (aux_type) & 0xff;
		return true;
	}


	/*
	 * extended attributes functions.
	 */
	#if defined(__APPLE__)
	ssize_t size_xattr(int fd, const char *xattr) {
		return fgetxattr(fd, xattr, NULL, 0, 0, 0);
	}

	ssize_t read_xattr(int fd, const char *xattr, void *buffer, size_t size) {
		return fgetxattr(fd, xattr, buffer, size, 0, 0);
	}

	ssize_t write_xattr(int fd, const char *xattr, const void *buffer, size_t size) {
		if (fsetxattr(fd, xattr, buffer, size, 0, 0) < 0) return -1;
		return size;
	}

	int remove_xattr(int fd, const char *xattr) {
		return fremovexattr(fd, xattr, 0);
	}

	#elif defined(__linux__) 
	ssize_t size_xattr(int fd, const char *xattr) {
		return fgetxattr(fd, xattr, NULL, 0);
	}

	ssize_t read_xattr(int fd, const char *xattr, void *buffer, size_t size) {
		return fgetxattr(fd, xattr, buffer, size);
	}

	ssize_t write_xattr(int fd, const char *xattr, const void *buffer, size_t size) {
		if (fsetxattr(fd, xattr, buffer, size, 0) < 0) return -1;
		return size;
	}

	int remove_xattr(int fd, const char *xattr) {
		return fremovexattr(fd, xattr);
	}

	#elif defined(__FreeBSD__)
	ssize_t size_xattr(int fd, const char *xattr) {
		return extattr_get_fd(fd, EXTATTR_NAMESPACE_USER, xattr, NULL, 0);
	}

	ssize_t read_xattr(int fd, const char *xattr, void *buffer, size_t size) {
		return extattr_get_fd(fd, EXTATTR_NAMESPACE_USER, xattr, buffer, size);
	}

	ssize_t write_xattr(int fd, const char *xattr, const void *buffer, size_t size) {
		return extattr_set_fd(fd, EXTATTR_NAMESPACE_USER, xattr, buffer, size);
	}

	int remove_xattr(int fd, const char *xattr) {
		return extattr_delete_fd(fd, EXTATTR_NAMESPACE_USER, xattr);
	}

	#elif defined(_AIX)
	ssize_t size_xattr(int fd, const char *xattr) {
		/*
		struct stat64x st;
		if (fstatea(fd, xattr, &st) < 0) return -1;
		return st.st_size;
		*/
		return fgetea(fd, xattr, NULL, 0);
	}

	ssize_t read_xattr(int fd, const char *xattr, void *buffer, size_t size) {
		return fgetea(fd, xattr, buffer, size);
	}

	ssize_t write_xattr(int fd, const char *xattr, const void *buffer, size_t size) {
		if (fsetea(fd, xattr, buffer, size, 0) < 0) return -1;
		return size;
	}

	int remove_xattr(int fd, const char *xattr) {
		return fremoveea(fd, xattr);
	}

	#endif








	int fi_open(const std::string &path, bool read_only) {

	#if defined(__sun__)
		if (read_only) return attropen(path.c_str(), XATTR_FINDERINFO_NAME, O_RDONLY);
		else return attropen(path.c_str(), XATTR_FINDERINFO_NAME, O_RDWR | O_CREAT, 0666);
	#elif defined(_WIN32)
		std::string s(path);
		s.append(":" XATTR_FINDERINFO_NAME);
		if (read_only) return open(s.c_str(), O_RDONLY | O_BINARY);
		else return open(s.c_str(), O_RDWR | O_CREAT | O_BINARY, 0666);
	#else

		return open(path.c_str(), O_RDONLY);
	#endif
	}

	int fi_write(int fd, const void *data, int length) {
		#if defined(__sun__) || defined(_WIN32)
			lseek(fd, 0, SEEK_SET);
			return write(fd, data, length);
		#else
			return write_xattr(fd, XATTR_FINDERINFO_NAME, data, length);
		#endif
	}

	int fi_read(int fd, void *data, int length) {
		#if defined(__sun__) || defined(_WIN32)
			lseek(fd, 0, SEEK_SET);
			return read(fd, data, length);
		#else
			return read_xattr(fd, XATTR_FINDERINFO_NAME, data, length);
		#endif
	}

#if defined(_WIN32)
void afp_init(struct AFP_Info *info) {
	//static_assert(sizeof(AFP_Info) == 60, "Incorrect AFP_Info size");
	memset(info, 0, sizeof(*info));
	info->magic = 0x00504641;
	info->version = 0x00010000;
	info->backup_date = 0x80000000;
}

int afp_verify(struct AFP_Info *info) {
	if (!info) return 0;

	if (info->magic != 0x00504641) return 0;
	if (info->version != 0x00010000) return 0;

	return 1;
}


int afp_to_filetype(struct AFP_Info *info, uint16_t *file_type, uint32_t *aux_type) {
	// check for prodos ftype/auxtype...
	if (info->prodos_file_type || info->prodos_aux_type) {
		*file_type = info->prodos_file_type;
		*aux_type = info->prodos_aux_type;
		return 0;
	}
	int ok = finder_info_to_filetype(info->finder_info, file_type, aux_type);
	if (ok == 0) {
		info->prodos_file_type = *file_type;
		info->prodos_aux_type = *aux_type;
	}
	return 0;
}

enum {
	trust_prodos,
	trust_hfs
};

void afp_synchronize(struct AFP_Info *info, int trust) {
	// if ftype/auxtype is inconsistent between prodos and finder info, use
	// prodos as source of truth.
	uint16_t f;
	uint32_t a;
	if (finder_info_to_filetype(info->finder_info, &f, &a) != 0) return;
	if (f == info->prodos_file_type && a == info->prodos_aux_type) return;
	if (trust == trust_prodos)
		file_type_to_finder_info(info->finder_info, info->prodos_file_type, info->prodos_aux_type);
	else {
		info->prodos_file_type = f;
		info->prodos_aux_type = a;
	}
}



#endif

}

finder_info_helper::finder_info_helper() {
#if defined(_WIN32)
	afp_init(&_afp);
#else
	memset(&_finder_info, 0, sizeof(_finder_info));
#endif
}

finder_info_helper::~finder_info_helper() {
	if (_fd >= 0) close(_fd);
}

bool finder_info_helper::open(const std::string &name, bool read_only) {

	if (_fd >= 0) {
		close(_fd);
		_fd = -1;
	}
	int fd = fi_open(name, read_only);
	if (fd < 0) return false;

	int ok = read(fd);
	// if write mode, it's ok if finder info doesn't exist (yet).
	if (!read_only && !ok) ok = true;

	if (read_only) close(fd);
	else _fd = fd;

	return ok;
}

bool finder_info_helper::read(int fd) {
#if defined(_WIN32)
	int ok = fi_read(fd, &_afp, sizeof(_afp));
	if (ok < sizeof(_afp) || !afp_verify(&_afp)) {
		afp_init(&_afp);
		return false;
	}
	if (!_afp.prodos_file_type && !_afp.prodos_aux_type)
		afp_synchronize(&_afp, trust_hfs);
#else
	int ok = fi_read(fd, &_finder_info, sizeof(_finder_info));
	if (ok < 0) {
		memset(&_finder_info, 0, sizeof(_finder_info));
		return false;
	}
	finder_info_to_filetype(_finder_info, &_prodos_file_type, &_prodos_aux_type);
#endif
	return true;
}

bool finder_info_helper::write(int fd) {
#if defined(_WIN32)
	return fi_write(fd, &_afp, sizeof(_afp));
#else
	return fi_write(fd, &_finder_info, sizeof(_finder_info));
#endif
}

bool finder_info_helper::write() {
	return write(_fd);
}


bool finder_info_helper::write(const std::string &name) {
	int fd = fi_open(name, false);
	if (fd < 0) return false;
	bool ok = write(fd);
	close(fd);
	return ok;
}



void finder_info_helper::set_prodos_file_type(uint16_t ftype, uint32_t atype) {
	_prodos_file_type = ftype;
	_prodos_aux_type = atype;
	file_type_to_finder_info(_finder_info, ftype, atype);
}


void finder_info_helper::set_prodos_file_type(uint16_t ftype) {
	set_prodos_file_type(ftype, _prodos_aux_type);
}

bool finder_info_helper::is_text() const {
	if (memcmp(_finder_info, "TEXT", 4) == 0) return true;
	if (_prodos_file_type == 0x04) return true;
	if (_prodos_file_type == 0xb0) return true;

	return false;
}


uint32_t finder_info_helper::file_type() const {
	uint32_t rv = 0;
	for (unsigned i = 0; i < 4; ++i) {
		rv <<= 8;
		rv |= _finder_info[i];
	}
	return rv;
}

uint32_t finder_info_helper::creator_type() const {
	uint32_t rv = 0;
	for (unsigned i = 4; i < 8; ++i) {
		rv <<= 8;
		rv |= _finder_info[i];
	}
	return rv;
}

void finder_info_helper::set_file_type(uint32_t x) {
	_finder_info[0] = x >> 24;
	_finder_info[1] = x >> 16;
	_finder_info[2] = x >> 8;
	_finder_info[3] = x >> 0;
}


void finder_info_helper::set_creator_type(uint32_t x) {
	_finder_info[4] = x >> 24;
	_finder_info[5] = x >> 16;
	_finder_info[6] = x >> 8;
	_finder_info[7] = x >> 0;
}

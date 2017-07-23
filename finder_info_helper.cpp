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
#include <windows.h>
#define XATTR_FINDERINFO_NAME "AFP_AfpInfo"
#endif

#ifndef XATTR_FINDERINFO_NAME
#define XATTR_FINDERINFO_NAME "com.apple.FinderInfo"
#endif


#if defined(_WIN32)
#define _prodos_file_type _afp.prodos_file_type
#define _prodos_aux_type _afp.prodos_aux_type
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
     $00        $0000           'pdos'  'BINA'
     $04 (TXT)  $0000           'pdos'  'TEXT'
     $FF (SYS)  (any)           'pdos'  'PSYS'
     $B3 (S16)  $DByz           'pdos'  'p' $B3 $DB $yz
     $B3 (S16)  (any)           'pdos'  'PS16'
     $D7        $0000           'pdos'  'MIDI'
     $D8        $0000           'pdos'  'AIFF'
     $D8        $0001           'pdos'  'AIFC'
     $E0        $0005           'dCpy'  'dImg'
     $FF (SYS)  (any)           'pdos'  'PSYS'
     $uv        $wxyz           'pdos'  'p' $uv $wx $yz


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


	template<class T>
	T _(const T t, std::error_code &ec) {
		if (t < 0) ec = std::error_code(errno, std::generic_category());
		return t;
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


	void set_or_throw_error(std::error_code *ec, int error, const std::string &what) {
		if (ec) *ec = std::error_code(error, std::system_category());
		else throw std::system_error(error, std::system_category(), what);
	}




#if defined(_WIN32)

	/*
	* allocating a new string could reset GetLastError() to 0.
	*/
	void set_or_throw_error(std::error_code *ec, const char *what) {
		auto e = GetLastError();
		set_or_throw_error(ec, e, what);
	}

	void set_or_throw_error(std::error_code *ec, const std::string &what) {
		auto e = GetLastError();
		set_or_throw_error(ec, e, what);
	}

	template<class ...Args>
	HANDLE CreateFileX(const std::string &s, Args... args) {
		return CreateFileA(s.c_str(), std::forward<Args>(args)...);
	}

	template<class ...Args>
	HANDLE CreateFileX(const std::wstring &s, Args... args) {
		return CreateFileW(s.c_str(), std::forward<Args>(args)...);
	}

	void fi_close(void *fd) {
		CloseHandle(fd);
	}

	void *fi_open(const std::string &path, int perm, std::error_code &ec) {

		ec.clear();

		std::string s(path);
		s.append(":" XATTR_FINDERINFO_NAME);

		HANDLE fh;
		bool ro = perm == 1;

		fh = CreateFileX(s, 
				ro ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ, 
				nullptr, 
				ro ? OPEN_EXISTING : OPEN_ALWAYS,
				ro ? FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL,
				nullptr);

		if (fh == INVALID_HANDLE_VALUE)
			set_or_throw_error(&ec, "CreateFile");

		return fh;
	}

	void *fi_open(const std::wstring &path, int perm, std::error_code &ec) {

		ec.clear();

		std::wstring s(path);
		s.append(L":" XATTR_FINDERINFO_NAME);

		HANDLE fh;
		bool ro = perm == 1;

		fh = CreateFileX(s,
			ro ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ,
			nullptr,
			ro ? OPEN_EXISTING : OPEN_ALWAYS,
			ro ? FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL,
			nullptr);

		if (fh == INVALID_HANDLE_VALUE)
			set_or_throw_error(&ec, "CreateFile");

		return fh;
	}

	int fi_write(void *handle, const void *data, int length, std::error_code &ec) {

		ec.clear();
		DWORD rv = 0;
		BOOL ok;

		LARGE_INTEGER zero = { 0 };

		ok = SetFilePointerEx(handle, zero, nullptr, FILE_BEGIN);
		if (!ok) {
			set_or_throw_error(&ec, "SetFilePointerEx");
			return 0;
		}
		ok = WriteFile(handle, data, length, &rv, nullptr);
		if (!ok) {
			set_or_throw_error(&ec, "WriteFile");
			return 0;
		}
		return rv;
	}

	int fi_read(void *handle, void *data, int length, std::error_code &ec) {

		ec.clear();
		DWORD rv = 0;
		BOOL ok;
		LARGE_INTEGER zero = { 0 };

		ok = SetFilePointerEx(handle, zero, nullptr, FILE_BEGIN);
		if (!ok) {
			set_or_throw_error(&ec, "SetFilePointerEx");
			return 0;
		}
		ok = ReadFile(handle, data, length, &rv, nullptr);
		if (!ok) {
			set_or_throw_error(&ec, "ReadFile");
			return 0;
		}
		return rv;
	}

#else

	void fi_close(int fd) {
		close(fd);
	}

	int fi_open(const std::string &path, int perm, std::error_code &ec) {

	#if defined(__sun__)
		if (perm == 1) return attropen(path.c_str(), XATTR_FINDERINFO_NAME, O_RDONLY);
		else return attropen(path.c_str(), XATTR_FINDERINFO_NAME, O_RDWR | O_CREAT, 0666);
	#else
		// linux needs to open as read/write to write it?
		//return open(path.c_str(), read_only ? O_RDONLY : O_RDWR);
		return open(path.c_str(), O_RDONLY);
	#endif
	}


	int fi_write(int fd, const void *data, int length, std::error_code &ec) {
		#if defined(__sun__)
			lseek(fd, 0, SEEK_SET);
			return write(fd, data, length);
		#else
			return write_xattr(fd, XATTR_FINDERINFO_NAME, data, length);
		#endif
	}

	int fi_read(int fd, void *data, int length, std::error_code &ec) {
		#if defined(__sun__)
			lseek(fd, 0, SEEK_SET);
			return read(fd, data, length);
		#else
			return read_xattr(fd, XATTR_FINDERINFO_NAME, data, length);
		#endif
	}

#endif


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
	uint16_t f = 0;
	uint32_t a = 0;
	if (finder_info_to_filetype(info->finder_info, &f, &a)) {
		if (f == info->prodos_file_type && a == info->prodos_aux_type) return;
	}
	
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

void finder_info_helper::close() {
#if _WIN32
	if (_fd != INVALID_HANDLE_VALUE) CloseHandle(_fd);
	_fd = INVALID_HANDLE_VALUE;
#else
	if (_fd >= 0) close(_fd);
	_fd = -1;
#endif

}
finder_info_helper::~finder_info_helper() {
	close();
}

bool finder_info_helper::open(const std::string &name, open_mode perm, std::error_code &ec) {

	ec.clear();

	close();

	if (perm < 1 || perm > 3) {
		ec = std::make_error_code(std::errc::invalid_argument);
		return false;
	}

	auto fd = fi_open(name, perm, ec);
	if (ec) return false;

	// win32 should read even if write-only.
	bool ok = read(_fd, ec);

	if (perm == read_only) {
		fi_close(fd);
		return ok;
	}

	// write mode, so it's ok if it doesn't exist.
	if (!ok) ec.clear();
	_fd = fd;
	return true;
}
#if _WIN32

bool finder_info_helper::open(const std::wstring &name, open_mode perm, std::error_code &ec) {

	ec.clear();

	close();

	if (perm < 1 || perm > 3) {
		ec = std::make_error_code(std::errc::invalid_argument);
		return false;
	}

	auto fd = fi_open(name, perm, ec);
	if (ec) return false;

	// win32 should read even if write-only.
	bool ok = read(_fd, ec);

	if (perm == read_only) {
		fi_close(fd);
		return ok;
	}

	// write mode, so it's ok if it doesn't exist.
	if (!ok) ec.clear();
	_fd = fd;
	return true;
}

bool finder_info_helper::read(void *fd, std::error_code &ec) {

	int ok = fi_read(fd, &_afp, sizeof(_afp), ec);
	if (ec) {
		afp_init(&_afp);
		return false;
	}
	if (ok < sizeof(_afp) || !afp_verify(&_afp)) {
		ec = std::make_error_code(std::errc::illegal_byte_sequence); // close enough!
		afp_init(&_afp);
		return false;
	}
	if (!_afp.prodos_file_type && !_afp.prodos_aux_type)
		afp_synchronize(&_afp, trust_hfs);

	return true;
}

bool finder_info_helper::write(void *fd, std::error_code &ec) {
	return fi_write(fd, &_afp, sizeof(_afp), ec) == sizeof(_afp);
}

#else

bool finder_info_helper::read(int fd, std::error_code &ec) {

	int ok = fi_read(fd, &_finder_info, sizeof(_finder_info), ec);
	if (ok < 0) {
		memset(&_finder_info, 0, sizeof(_finder_info));
		return false;
	}
	finder_info_to_filetype(_finder_info, &_prodos_file_type, &_prodos_aux_type);
	return true;
}

bool finder_info_helper::write(int fd, std::error_code &ec) {
	return fi_write(fd, &_finder_info, sizeof(_finder_info), ec) == sizeof(_finder_info);
}

#endif

bool finder_info_helper::write(std::error_code &ec) {
	ec.clear();
	return write(_fd, ec);
}


bool finder_info_helper::write(const std::string &name, std::error_code &ec) {
	ec.clear();
	auto fd = fi_open(name, write_only, ec);

	if (ec)
		return false;

	bool ok = write(fd, ec);
	fi_close(fd);
	return ok;
}

#ifdef _WIN32
bool finder_info_helper::write(const std::wstring &name, std::error_code &ec) {
	ec.clear();
	auto fd = fi_open(name, write_only, ec);

	if (ec)
		return false;

	bool ok = write(fd, ec);
	fi_close(fd);
	return ok;
}

#endif

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

bool finder_info_helper::is_binary() const {
	if (is_text()) return false;
	if (_prodos_file_type || _prodos_aux_type) return true;

	if (memcmp(_finder_info, "\x00\x00\x00\x00\x00\x00\x00\x00", 8) == 0) return false;
	return true;
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

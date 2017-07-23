#include "mapped_file.h"
#include <memory>
#include <functional>
#include <system_error>

#include "unique_resource.h"

namespace {

	void set_or_throw_error(std::error_code *ec, int error, const std::string &what) {
		if (ec) *ec = std::error_code(error, std::system_category());
		else throw std::system_error(error, std::system_category(), what);
	}

}

#ifdef _WIN32
#include <windows.h>

namespace {

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

}

void mapped_file_base::close() {
	if (is_open()) {

		UnmapViewOfFile(_data);
		CloseHandle(_map_handle);
		CloseHandle(_file_handle);
		reset();
	}
}

template<class T>
void mapped_file_base::open_common(const T& p, mapmode flags, size_t length, size_t offset, std::error_code *ec) {

	if (ec) ec->clear();

	HANDLE fh;
	HANDLE mh;

	// length of 0 in CreateFileMapping / MapViewOfFile
	// means map the entire file.

	if (is_open()) close();

	fh = CreateFileX(p, 
		flags == readonly ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ, 
		nullptr,
		OPEN_EXISTING, 
		flags == readonly ? FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL,
		nullptr
	);
	if (fh == INVALID_HANDLE_VALUE)
		return set_or_throw_error(ec, "CreateFile");

	auto fh_close = make_unique_resource(fh, CloseHandle);


	if (length == -1) {
		LARGE_INTEGER file_size;
		GetFileSizeEx(fh, &file_size);
		length = (size_t)file_size.QuadPart;
	}

	if (length == 0) return;

	DWORD protect = 0;
	DWORD access = 0;
	switch (flags) {
	case readonly:
		protect = PAGE_READONLY;
		access = FILE_MAP_READ;
		break;
	case readwrite:
		protect = PAGE_READWRITE;
		access = FILE_MAP_WRITE;
		break;
	case priv:
		protect = PAGE_WRITECOPY;
		access = FILE_MAP_COPY;
		break;
	}

	mh = CreateFileMapping(fh, nullptr, protect, 0, 0, 0);
	if (mh == INVALID_HANDLE_VALUE)
		return set_or_throw_error(ec, "CreateFileMapping");

	auto mh_close = make_unique_resource(mh, CloseHandle);


	ULARGE_INTEGER ll;
	ll.QuadPart = offset;

	_data = MapViewOfFileEx(mh, 
		access, 
		ll.HighPart,
		ll.LowPart,
		length, 
		nullptr);
	if (!_data)
		return set_or_throw_error(ec, "MapViewOfFileEx");


	_file_handle = fh_close.release();
	_map_handle = mh_close.release();
	_size = length;
	_flags = flags;
}

void mapped_file_base::open(const std::string &p, mapmode flags, size_t length, size_t offset, std::error_code *ec) {
	open_common(p, flags, length, offset, ec);
}

void mapped_file_base::open(const std::wstring &p, mapmode flags, size_t length, size_t offset, std::error_code *ec) {
	open_common(p, flags, length, offset, ec);
}




template<class T>
void mapped_file_base::create_common(const T& p, size_t length, std::error_code *ec) {

	if (ec) ec->clear();

	const size_t offset = 0;

	HANDLE fh;
	HANDLE mh;
	LARGE_INTEGER file_size;

	const DWORD protect = PAGE_READWRITE;
	const DWORD access = FILE_MAP_WRITE;


	if (is_open()) close();

	fh = CreateFileX(p, 
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ, 
		nullptr,
		CREATE_ALWAYS, 
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);
	if (fh == INVALID_HANDLE_VALUE)
		return set_or_throw_error(ec, "CreateFile");

	auto fh_close = make_unique_resource(fh, CloseHandle);

	if (length == 0) return;

	file_size.QuadPart = length;
	if (!SetFilePointerEx(fh, file_size, nullptr, FILE_BEGIN))
		return set_or_throw_error(ec, "SetFilePointerEx");

	if (!SetEndOfFile(fh))
		return set_or_throw_error(ec, "SetEndOfFile");

	mh = CreateFileMapping(fh, nullptr, protect, 0, 0, 0);
	if (mh == INVALID_HANDLE_VALUE)
		return set_or_throw_error(ec, "CreateFileMapping");

	auto mh_close = make_unique_resource(mh, CloseHandle);

	ULARGE_INTEGER ll;
	ll.QuadPart = offset;

	_data = MapViewOfFileEx(mh, 
		access, 
		ll.HighPart,
		ll.LowPart,
		length, 
		nullptr);

	if (!_data)
		return set_or_throw_error(ec, "MapViewOfFileEx");

	_file_handle = fh_close.release();
	_map_handle = mh_close.release();

	_size = length;
	_flags = readwrite;
}

void mapped_file_base::create(const std::string &p, size_t length, std::error_code *ec) {
	create_common(p, length, ec);
}
void mapped_file_base::create(const std::wstring &p, size_t length, std::error_code *ec) {
	create_common(p, length, ec);
}

#else

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cerrno>

namespace {


	void set_or_throw_error(std::error_code *ec, const char *what) {
		set_or_throw_error(ec, errno, what);
	}

	void set_or_throw_error(std::error_code *ec, const std::string &what) {
		set_or_throw_error(ec, errno, what);
	}


}

void mapped_file_base::close() {
	if (is_open()) {
		::munmap(_data, _size);
		::close(_fd);
		reset();
	}
}


void mapped_file_base::open(const std::string& p, mapmode flags, size_t length, size_t offset, std::error_code *ec) {

	if (ec) ec->clear();

	int fd;

	int oflags = 0;

	if (is_open()) close();

	switch (flags) {
	case readonly:
		oflags = O_RDONLY;
		break;
	default:
		oflags = O_RDWR;
		break;
	}

	fd = ::open(p.c_str(), oflags);
	if (fd < 0) {
		return set_or_throw_error(ec, "open");
	}

	auto close_fd = make_unique_resource(fd, ::close);


	if (length == -1) {
		struct stat st;

		if (::fstat(fd, &st) < 0) {
			set_or_throw_error(ec, "stat");
			return;
		}
		length = st.st_size;
	}

	if (length == 0) return;

	_data = ::mmap(0, length, 
		flags == readonly ? PROT_READ : PROT_READ | PROT_WRITE, 
		flags == priv ? MAP_PRIVATE : MAP_SHARED, 
		fd, offset);

	if (_data == MAP_FAILED) {
		_data = nullptr;
		return set_or_throw_error(ec, "mmap");
	}

	_fd = close_fd.release();
	_size = length;
	_flags = flags;
}

void mapped_file_base::create(const std::string& p, size_t length, std::error_code *ec) {

	if (ec) ec->clear();

	int fd;
	const size_t offset = 0;

	if (is_open()) close();

	fd = ::open(p.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (fd < 0) {
		return set_or_throw_error(ec, "open");
	}


	auto close_fd = make_unique_resource(fd, ::close);


	if (length == 0) return;

	if (::ftruncate(fd, length) < 0) {
		return set_or_throw_error(ec, "ftruncate");
	}


	_data = ::mmap(0, length, 
		PROT_READ | PROT_WRITE, 
		MAP_SHARED, 
		fd, offset);

	if (_data == MAP_FAILED) {
		_data = nullptr;
		return set_or_throw_error(ec, "mmap");
	}

	_fd = close_fd.release();
	_size = length;
	_flags = readwrite;
}



#endif


void mapped_file_base::reset() {
	_data = nullptr;
	_size = 0;
	_flags = readonly;
#ifdef _WIN32
	_file_handle = nullptr;
	_map_handle = nullptr;
#else
	_fd = -1;
#endif
}

void mapped_file_base::swap(mapped_file_base &rhs)
{
	if (std::addressof(rhs) != this) {
		std::swap(_data, rhs._data);
		std::swap(_size, rhs._size);
		std::swap(_flags, rhs._flags);
#ifdef _WIN32
		std::swap(_file_handle, rhs._file_handle);
		std::swap(_map_handle, rhs._map_handle);
#else
		std::swap(_fd, rhs._fd);
#endif
	}
}

mapped_file::mapped_file(mapped_file &&rhs) : mapped_file() {
	swap(rhs);
	//rhs.reset();
}

mapped_file& mapped_file::operator=(mapped_file &&rhs) {
	if (std::addressof(rhs) == this) return *this;

	swap(rhs);
	rhs.close();
	//rhs.reset();
	return *this;
}



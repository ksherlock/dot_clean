#ifndef __mapped_file_h__
#define __mapped_file_h__

#include <cstddef>
#include <system_error>
#include <string>

class mapped_file_base {
public:


	enum mapmode { readonly, readwrite, priv };
	enum createmode { truncate, exclusive };

	void close();

	bool is_open() const {
		return _data != nullptr;
	}

	size_t size() const {
		return _size;
	}

	operator bool() const { return is_open(); }
	bool operator !() const { return !is_open(); }

	~mapped_file_base() { close(); }

protected:

	void swap(mapped_file_base &rhs);

	void open(const std::string &p, mapmode flags, size_t length, size_t offset, std::error_code *ec);
	void create(const std::string &p, size_t new_size, std::error_code *ec); // always creates readwrite.

#ifdef _WIN32

	void open(const std::wstring &p, mapmode flags, size_t length, size_t offset, std::error_code *ec);
	void create(const std::wstring &p, size_t new_size, std::error_code *ec); // always creates readwrite.

	template<class S>
	void open_common(const S &p, mapmode flags, size_t length, size_t offset, std::error_code *ec);

	template<class S>
	void create_common(const S &p, size_t new_size, std::error_code *ec); // always creates readwrite.

#endif

	void reset();


	size_t _size = 0;
	void *_data = nullptr;
	mapmode _flags = readonly;

#ifdef _WIN32
	void *_file_handle = nullptr;
	void *_map_handle = nullptr;
#else
	int _fd = -1;
#endif
};



class mapped_file : public mapped_file_base {

	typedef mapped_file_base base;

public:

	typedef unsigned char value_type;

	typedef value_type *iterator;
	typedef const value_type *const_iterator;

	typedef value_type &reference ;
	typedef const value_type &const_reference;


	mapped_file() = default;
	mapped_file(const std::string &p, mapmode flags = readonly, size_t length = -1, size_t offset = 0) {
		open(p, flags, length, offset);
	}

	mapped_file(const std::string &p, std::error_code &ec) noexcept {
		open(p, readonly, -1, 0, ec);
	}
	mapped_file(const std::string &p, mapmode flags, std::error_code &ec) noexcept {
		open(p, flags, -1, 0, ec);
	}

	mapped_file(const std::string &p, mapmode flags, size_t length, std::error_code &ec) noexcept {
		open(p, flags, length, 0, ec);
	}

	mapped_file(const std::string &p, mapmode flags, size_t length, size_t offset, std::error_code &ec) noexcept {
		open(p, flags, length, offset, ec);
	}


	mapped_file(mapped_file &&);
	mapped_file(const mapped_file &) = delete;

	mapped_file &operator=(mapped_file &&);
	mapped_file &operator=(const mapped_file &) = delete;


	void open(const std::string &p, mapmode flags, size_t length = -1, size_t offset = 0) {
		base::open(p, flags, length, offset, nullptr);
	}
	void open(const std::string &p, std::error_code &ec) noexcept {
		base::open(p, readonly, -1, 0, &ec);
	}
	void open(const std::string &p, mapmode flags, std::error_code &ec) noexcept {
		base::open(p, flags, -1, 0, &ec);
	}
	void open(const std::string &p, mapmode flags, size_t length, std::error_code &ec) noexcept {
		base::open(p, flags, length, 0, &ec);
	}
	void open(const std::string &p, mapmode flags, size_t length, size_t offset, std::error_code &ec) noexcept {
		base::open(p, flags, length, offset, &ec);
	}

#ifdef _WIN32
	void open(const std::wstring &p, mapmode flags, size_t length = -1, size_t offset = 0) {
		base::open(p, flags, length, offset, nullptr);
	}
	void open(const std::wstring &p, std::error_code &ec) noexcept {
		base::open(p, readonly, -1, 0, &ec);
	}
	void open(const std::wstring &p, mapmode flags, std::error_code &ec) noexcept {
		base::open(p, flags, -1, 0, &ec);
	}
	void open(const std::wstring &p, mapmode flags, size_t length, std::error_code &ec) noexcept {
		base::open(p, flags, length, 0, &ec);
	}
	void open(const std::wstring &p, mapmode flags, size_t length, size_t offset, std::error_code &ec) noexcept {
		base::open(p, flags, length, offset, &ec);
	}

#endif

	void create(const std::string &p, size_t size) {
		base::create(p, size, nullptr);
	}
	void create(const std::string &p, size_t size, std::error_code &ec)  noexcept {
		base::create(p, size, &ec);
	}

#ifdef _WIN32
	void create(const std::wstring &p, size_t size) {
		base::create(p, size, nullptr);
	}
	void create(const std::wstring &p, size_t size, std::error_code &ec)  noexcept {
		base::create(p, size, &ec);
	}
#endif


	const value_type *data() const {
		return (const value_type *)_data;
	}

	value_type *data() {
		return (value_type *)_data;
	}

	const_iterator cbegin() const {
		return data();
	}

	const_iterator cend() const {
		return data() + size();
	}

	const_iterator begin() const {
		return cbegin();
	}
	
	const_iterator end() const {
		return cend();
	}




	iterator begin() {
		return (iterator)_data;
	}

	iterator end() {
		return (iterator)_data + size();
	}

	mapmode flags() const {
		return _flags;
	}
	
	void swap(mapped_file &rhs) {
		base::swap(rhs);
	}

};

namespace std {
	template<class T>
	void swap(mapped_file &a, mapped_file &b) {
		a.swap(b);
	}
}

#endif

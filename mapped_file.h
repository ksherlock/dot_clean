#ifndef __mapped_file_h__
#define __mapped_file_h__

#include <string>

#include <cstddef>
#include <system_error>

class mapped_file_base {
public:

	typedef std::string path_type;

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

	void open(const path_type& p, mapmode flags, size_t length, size_t offset, std::error_code *ec);
	void create(const path_type &p, size_t new_size, std::error_code *ec); // always creates readwrite.
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
	mapped_file(const path_type& p, mapmode flags = readonly, size_t length = -1, size_t offset = 0) {
		open(p, flags, length, offset);
	}

	mapped_file(const path_type &p, std::error_code &ec) noexcept {
		open(p, readonly, -1, 0, ec);
	}
	mapped_file(const path_type &p, mapmode flags, std::error_code &ec) noexcept {
		open(p, flags, -1, 0, ec);
	}

	mapped_file(const path_type &p, mapmode flags, size_t length, std::error_code &ec) noexcept {
		open(p, flags, length, 0, ec);
	}

	mapped_file(const path_type &p, mapmode flags, size_t length, size_t offset, std::error_code &ec) noexcept {
		open(p, flags, length, offset, ec);
	}


	mapped_file(mapped_file &&);
	mapped_file(const mapped_file &) = delete;

	mapped_file &operator=(mapped_file &&);
	mapped_file &operator=(const mapped_file &) = delete;


	void open(const path_type& p, mapmode flags, size_t length = -1, size_t offset = 0) {
		base::open(p, flags, length, offset, nullptr);
	}

	void open(const path_type &p, std::error_code &ec) noexcept {
		base::open(p, readonly, -1, 0, &ec);
	}
	void open(const path_type &p, mapmode flags, std::error_code &ec) noexcept {
		base::open(p, flags, -1, 0, &ec);
	}
	void open(const path_type &p, mapmode flags, size_t length, std::error_code &ec) noexcept {
		base::open(p, flags, length, 0, &ec);
	}
	void open(const path_type &p, mapmode flags, size_t length, size_t offset, std::error_code &ec) noexcept {
		base::open(p, flags, length, offset, &ec);
	}

	void create(const path_type &p, size_t size) {
		base::create(p, size, nullptr);
	}

	void create(const path_type &p, size_t size, std::error_code &ec)  noexcept {
		base::create(p, size, &ec);
	}


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


#include <stdint.h>
#include <string>

#include <system_error>


#if defined(_WIN32)
#pragma pack(push, 2)
struct AFP_Info {
	uint32_t magic;
	uint32_t version;
	uint32_t file_id;
	uint32_t backup_date;
	uint8_t finder_info[32];
	uint16_t prodos_file_type;
	uint32_t prodos_aux_type;
	uint8_t reserved[6];
};
#pragma pack(pop)

#endif

class finder_info_helper {

public:

	enum open_mode {
		read_only = 1,
		write_only = 2,
		read_write = 3,
	};

	finder_info_helper();
	~finder_info_helper();

	finder_info_helper(const finder_info_helper &) = delete;
	finder_info_helper(finder_info_helper &&) = delete;

	finder_info_helper& operator=(const finder_info_helper &) = delete;
	finder_info_helper& operator=(finder_info_helper &&) = delete;


	const uint8_t *finder_info() const {
#if defined(_WIN32)
		return _afp.finder_info;
#else
		return _finder_info;
#endif
	}

	uint8_t *finder_info() {
#if defined(_WIN32)
		return _afp.finder_info;
#else
		return _finder_info;
#endif		
	}


	bool read(const std::string &fname, std::error_code &ec) {
		return open(fname, read_only, ec);
	}

	bool write(const std::string &fname, std::error_code &ec);

	bool open(const std::string &fname, open_mode perm, std::error_code &ec);
	bool open(const std::string &fname, std::error_code &ec) {
		return open(fname, read_only, ec);
	}


#if defined(_WIN32)
	bool read(const std::wstring &pathName, std::error_code &ec) {
		return open(pathName, read_only, ec);
	}

	bool write(const std::wstring &pathName, std::error_code &ec);

	bool open(const std::wstring &fname, open_mode perm, std::error_code &ec);
	bool open(const std::wstring &fname, std::error_code &ec) {
		return open(fname, read_only, ec);
	}

#endif


	bool write(std::error_code &ec);

	uint32_t creator_type() const;
	uint32_t file_type() const;

	uint16_t prodos_file_type() const {
		#if defined(_WIN32)
		return _afp.prodos_file_type;
		#else
		return _prodos_file_type;
		#endif
	}

	uint32_t prodos_aux_type() const {
		#if defined(_WIN32)
		return _afp.prodos_aux_type;
		#else
		return _prodos_aux_type;
		#endif		
	}

	void set_prodos_file_type(uint16_t);
	void set_prodos_file_type(uint16_t, uint32_t);

	void set_file_type(uint32_t);
	void set_creator_type(uint32_t);

	bool is_text() const;
	bool is_binary() const;

private:

	void close();

#if defined(_WIN32)
	bool write(void *handle, std::error_code &ec);
	bool read(void *handle, std::error_code &ec);
#else
	bool write(int fd, std::error_code &ec);
	bool read(int fd, std::error_code &ec);
#endif

	#if defined(_WIN32)
	void *_fd = (void *)-1;
	AFP_Info _afp;
	#else
	int _fd = -1;

	uint16_t _prodos_file_type = 0;
	uint32_t _prodos_aux_type = 0;
	uint8_t _finder_info[32] = {};
	#endif
};


#include <stdint.h>
#include <string>

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


	bool read(const std::string &fname);
	bool write(const std::string &fname);
	bool open(const std::string &fname, bool read_only = true);
	bool write();

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

private:

	bool write(int fd);
	bool read(int fd);

	int _fd = -1;

	#if defined(_WIN32)
	AFP_Info _afp;
	#else
	uint16_t _prodos_file_type = 0;
	uint32_t _prodos_aux_type = 0;
	uint8_t _finder_info[32] = {};
	#endif
};

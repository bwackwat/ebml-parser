#include <iostream>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <bitset>
#include <vector>
#include <array>
#include <chrono>
#include <iomanip>

#include <unistd.h>
#include <time.h>

#define BUFSIZE 1048576 // 1MB

enum ebml_element_type {
	MASTER,
	UINT,
	INT,
	STRING,
	UTF8,
	BINARY,
	FLOAT,
	DATE
};

class simple_vint{
public:
	uint8_t width;
	uint8_t data[8];

	bool is_all_ones(){
		for(int i = 1; i < width - 1; i++){
			if(data[i] != 255){
				return false;
			}
		}
		return true;
	}

	uint64_t get_uint(){
		uint64_t value = 0;
		value = data[width - 1];
		for(int i = width - 1; i > 0; --i){
			value += ((uint64_t)data[i - 1] << ((width - i) * 8));
		}
		return value;
	}

	// Unused, good point though.
	uint64_t get_little_endian_uint(){
		uint64_t value = 0;
		value = data[0];
		for(int i = 0; i < width - 1; ++i){
			value += ((uint64_t)data[i + 1] << (i * 8));
		}
		return value;
	}
};

class ebml_element{
public:
	std::string name;
	std::array<uint8_t, 4> id;
	enum ebml_element_type type;

	ebml_element(std::string name, std::array<uint8_t, 4> const& id, enum ebml_element_type type)
	:name(name), id(id), type(type){}
};

const int SPEC_LEN = 250;

#include "spec.cpp"

ebml_element* get_element(std::array<uint8_t, 4> id, uint8_t level){
	bool found;
	for(int i = 0; i < SPEC_LEN; ++i){
		found = true;
		for(int j = 0; j < level; ++j){
			if(ebml_spec[i]->id[j] != id[j]){
				found = false;
				break;
			}
		}
		if(found){
			return ebml_spec[i];
		}
	}
	return 0;
}

bool verbose = false;
typedef std::ostream& (*manip) (std::ostream&);
class verbose_log{};
template <class T> verbose_log& operator<< (verbose_log& l, const T& x){
	if(verbose)
		std::cout << x;
	return l;
}
verbose_log& operator<< (verbose_log& l, manip manipulator){
	if(verbose)
		std::cout << manipulator;
	return l;
}
verbose_log vlog;

class ebml_parser{
public:
void parse(int fd){
	int len, mask, pos = 0;
	uint8_t buffer[BUFSIZE];
	std::bitset<8> bits;

	while(1){
		// Get EBML Element ID first byte.
		if((len = read(fd, buffer, 1)) < 0){
			std::cout << "Uh oh, read first id byte error!\n";
			break;
		}else if(len == 0){
			std::cout << "DONE!" << std::endl;
			break;
		}
		pos++;

		if(buffer[0] == 0){
			std::cout << "Read '0' byte..." << std::endl;
			continue;
		}

		bits = std::bitset<8>(buffer[0]);
		vlog << "Element ID First Byte: " << bits << std::endl;

		simple_vint id;
		id.width = 1;
		mask = 0x80;
		// Get EBML Element ID vint width.
		while(!(buffer[0] & mask)){
			mask >>= 1;
			id.width++;
		}

		id.data[0] = buffer[0];
		// Get EBML Element ID vint data.
		if((len = read(fd, buffer, id.width - 1)) != id.width - 1){
			std::cout << "Uh oh, read id data error!\n";
			break;
		}
		pos += id.width - 1;
		vlog << "Element ID Bytes: " << bits;
		// Get EBML Element ID.
		for(int i = 1; i < id.width; ++i){
			id.data[i] = buffer[i - 1];
			bits = std::bitset<8>(buffer[i]);
			vlog << ' ' << bits;
		}
		vlog << std::endl;
		if(verbose){
			vlog << "Element ID: 0x";
			for(int i = 0; i < id.width; ++i){
				vlog << std::hex << (int)id.data[i];
			}
			vlog << std::endl;
		}

		// Get EBML Element Size first byte.
		if((len = read(fd, buffer, 1)) != 1){
			std::cout << "Uh oh, read first size byte error!\n";
			break;
		}
		pos++;

		bits = std::bitset<8>(buffer[0]);
		vlog << "Element Size First Byte: " << bits << std::endl;

		simple_vint size;
		size.width = 1;
		mask = 0x80;
		// Get EBML Element Size vint width.
		while(!(buffer[0] & mask)){
			mask >>= 1;
			size.width++;
		}

		buffer[0] ^= mask;
		size.data[0] = buffer[0];
		// Get EBML Element Size vint data.
		if((len = read(fd, buffer, size.width - 1)) != size.width - 1){
			std::cout << "Uh oh, read id data error!\n";
			break;
		}
		pos += size.width - 1;
		bits = std::bitset<8>(size.data[0]);
		vlog << "Element Size Bytes: " << bits;
		// Get EBML Element Size.
		for(int i = 1; i < size.width; ++i){
			size.data[i] = buffer[i - 1];
			bits = std::bitset<8>(buffer[i]);
			vlog << ' ' << bits;
		}
		vlog << std::endl;
		vlog << "Element Size: " << std::dec << size.get_uint() << std::endl;

		// Specification for ID lookup.
		ebml_element* e = get_element(
			{{id.data[0], id.data[1], id.data[2], id.data[3]}},
			id.width);

		vlog << "--------------------------------------------------------" << std::endl;
		if(e != 0){
			if(e->type != MASTER){
				// Get EBML Element Data, parse it.
				uint64_t data_len = size.get_uint();
				if((len = read(fd, buffer, data_len) != data_len)){
					std::cout << "Uh oh, could not read all the data!" << std::endl;
					std::cout << "Wanted " << data_len << " found " << len << std::endl;
					break;
				}
				pos += data_len;
				std::cout << '(' << std::dec << pos << ") " << e->name << ": ";
				if(e->type == STRING || e->type == UTF8){
					for(int i = 0; i < data_len; ++i){
						std::cout << buffer[i];
					}
					std::cout << std::endl;
				}else if(e->type == BINARY){
					for(int i = 0; i < data_len; ++i){
						// I'll only care about the first 32 binary bytes.
						if(i == 32 && !verbose){
							std::cout << "...";
							break;
						}
						std::cout << std::hex << (int)buffer[i];
					}
					std::cout << std::endl;
					if(e->name == "SimpleBlock" || e->name == "Block"){
						bits = std::bitset<8>(buffer[0]);
						vlog << "Block First Byte: " << bits << std::endl;
						simple_vint track_number;
						track_number.width = 1;
						mask = 0x80;
						while(!(buffer[0] & mask)){
							mask >>= 1;
							track_number.width++;
						}
						buffer[0] ^= mask;
						vlog << "Block Track Number Bytes: " << std::endl;
						for(int i = 0; i < track_number.width; ++i){
							bits = std::bitset<8>(buffer[i]);
							vlog << bits << ' ';
							track_number.data[i] = buffer[i];
						}
						std::cout << "Track Number: " << std::dec << (int)track_number.get_uint() << std::endl;
						int16_t timecode = (int16_t)(((uint16_t)buffer[track_number.width] << 8) | buffer[track_number.width + 1]);
						std::cout << "Timecode: " << std::dec << (int)timecode << std::endl;
					}
				}else if(e->type == UINT){
					simple_vint data;
					data.width = 0;
					for(int i = 0; i < data_len; ++i){
						data.data[i] = buffer[i];
						data.width++;
					}
					uint64_t val = data.get_uint();
					std::cout << std::dec << val;
					if(e->id[0] == 0x83){
						if(val == 1){
							std::cout << " (video)";
						}else if(val == 2){
							std::cout << " (audio)";
						}else if(val == 3){
							std::cout << " (complex)";
						}
					}
					std::cout << std::endl;
				}else if(e->type == FLOAT){
					simple_vint data;
					data.width = 0;
					for(int i = 0; i < data_len; ++i){
						data.data[i] = buffer[i];
						data.width++;
					}
					int64_t int_val = int64_t(data.get_uint());
					if(data.width == 4){
						float float_val;
						memcpy(&float_val, &int_val, 4);
						std::cout << std::fixed << std::dec << float_val;
					}else if(data.width == 8){
						double double_val;
						memcpy(&double_val, &int_val, 8);
						std::cout << std::fixed << std::dec << double_val;
					}else{
						std::cout << "Bad float width:: " << std::dec << (int)data.width;
					}
					std::cout << std::endl;
				}else if(e->type == DATE){
					simple_vint data;
					data.width = 0;
					for(int i = 0; i < data_len; ++i){
						data.data[i] = buffer[i];
						data.width++;
					}
					// This is a time_t struct composed of:
					// time point 0 (Jan 1 1970)
					// nanoseconds to millenium (Jan 1 2001)
					// nanoseconds to date file was created.
					std::time_t date_val = std::chrono::system_clock::to_time_t(std::chrono::system_clock::time_point::time_point() + std::chrono::nanoseconds(978307200000000000) + std::chrono::nanoseconds(int64_t(data.get_uint())));
					// This is the local time; PST in San Francisco, -7h or something.)
					//std::cout << std::ctime(&date_val);
					// This is the UTC time.
					std::cout << asctime(gmtime(&date_val));
				}else if(e->type == INT){
					simple_vint data;
					data.width = 0;
					for(int i = 0; i < data_len; ++i){
						data.data[i] = buffer[i];
						data.width++;
					}
					std::cout << std::dec << int64_t(data.get_uint()) << std::endl;
				}else{
					for(int i = 0; i < data_len; ++i){
						std::cout << std::hex << (int)buffer[i];
					}
					std::cout << std::endl;
				}
			}else{
				// Master data is actually just more elements, continue.
				std::cout << '(' << std::dec << pos << ')' << " ----- " << e->name << " [";
				if(size.is_all_ones()){
					std::cout << "Unknown";
				}else{
					std::cout << size.get_uint();
				}
				std::cout << ']' << std::endl;
			}
		}else{
			std::cout << "UNKNOWN ELEMENT!" << std::endl;
		}
		vlog << "--------------------------------------------------------" << std::endl;
	}
}
};

int main(int argc, char** argv){
	for(int i = 0; i < argc; i++){
                if(std::strcmp(argv[i], "-v") == 0){
                        verbose = true;
                }
	}

	ebml_parser p;
	p.parse(STDIN_FILENO);

	return 0;
}

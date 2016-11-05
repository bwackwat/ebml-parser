#include <iostream>
#include <cstring>
#include <cstdlib>
#include <bitset>
#include <vector>
#include <array>

#include <unistd.h>

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

	uint64_t get_uint(){
		uint64_t value = 0;
		value = data[width - 1];
		for(int i = width - 1; i > 0; --i){
			value += ((uint64_t)data[i - 1] << ((width - i) * 8));
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

int main(int argc, char** argv){
	int len, mask, elems = 0;
	uint8_t buffer[BUFSIZE];
	std::bitset<8> bits;

	for(int i = 0; i < argc; i++){
                if(std::strcmp(argv[i], "-v") == 0){
                        verbose = true;
                }
	}

	while(1){
		// Get EBML Element ID first byte.
		if((len = read(STDIN_FILENO, buffer, 1)) < 0){
			std::cout << "Uh oh, read first id byte error!\n";
			break;
		}else if(len == 0){
			vlog << "DONE!" << std::endl;
			break;
		}

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
		if((len = read(STDIN_FILENO, buffer, id.width - 1)) != id.width - 1){
			std::cout << "Uh oh, read id data error!\n";
			break;
		}
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
		if((len = read(STDIN_FILENO, buffer, 1)) != 1){
			std::cout << "Uh oh, read first size byte error!\n";
			break;
		}

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
		if((len = read(STDIN_FILENO, buffer, size.width - 1)) != size.width - 1){
			std::cout << "Uh oh, read id data error!\n";
			break;
		}
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
				if((len = read(STDIN_FILENO, buffer, data_len) != data_len)){
					std::cout << "Uh oh, could not read all the data!" << std::endl;
					std::cout << "Wanted " << data_len << " found " << len << std::endl;
					break;
				}
				std::cout << e->name << ": ";
				if(e->type == STRING || e->type == UTF8){
					for(int i = 0; i < data_len; ++i){
						std::cout << buffer[i];
					}
				}else if(e->type == BINARY){
					for(int i = 0; i < data_len; ++i){
						// I'll only care about the first 32 binary bytes.
						if(i == 32 && !verbose){
							std::cout << "...";
							break;
						}
						std::cout << std::hex << (int)buffer[i];
					}
					if(e->name == "SimpleBlock" || e->name == "Block"){
						bits = std::bitset<8>(buffer[0]);
						vlog << std::endl << "Block First Byte: " << bits;
						simple_vint track_number;
						track_number.width = 1;
						mask = 0x80;
						while(!(buffer[0] & mask)){
							mask >>= 1;
							track_number.width++;
						}
						buffer[0] ^= mask;
						vlog << std::endl << "Block Track Number Bytes: ";
						for(int i = 0; i < track_number.width; ++i){
							bits = std::bitset<8>(buffer[i]);
							vlog << bits << ' ';
							track_number.data[i] = buffer[i];
						}
						std::cout << std::endl << "Track Number: " << std::dec << (int)track_number.get_uint();
						int16_t timecode = (int16_t)(((uint16_t)buffer[track_number.width] << 8) | buffer[track_number.width + 1]);
						std::cout << std::endl << "Timecode: " << std::dec << (int)timecode;
					}
				}else if(e->type == UINT){
					simple_vint data;
					data.width = 0;
					for(int i = 0; i < data_len; ++i){
						data.data[i] = buffer[i];
						data.width++;
					}
					std::cout << std::dec << data.get_uint();
				}else if(e->type == INT || e->type == DATE || e->type == FLOAT){
					simple_vint data;
					data.width = 0;
					for(int i = 0; i < data_len; ++i){
						data.data[i] = buffer[i];
						data.width++;
					}
					std::cout << std::dec << static_cast<int64_t>(data.get_uint());
				}else{
					for(int i = 0; i < data_len; ++i){
						std::cout << std::hex << (int)buffer[i];
					}
				}
				std::cout << std::endl;
			}else{
				// Master data is actually just more elements, continue.
				std::cout << " - " << e->name << std::endl;
			}
		}else{
			std::cout << "UNKNOWN ELEMENT!" << std::endl;
		}
		vlog << "--------------------------------------------------------" << std::endl;
		elems++;
		if(elems > 30){
			//break;
		}
	}

	return 0;
}

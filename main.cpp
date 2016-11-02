#include <iostream>
#include <cstring>
#include <cstdlib>
#include <bitset>
#include <vector>
#include <array>

#include <unistd.h>

#define BUFSIZE 1000000 //1mb

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

	uint64_t get_int(){
		uint64_t value = 0;
		value = data[width - 1];
		for(int i  = width - 1; i > 0; --i){
			value += ((uint32_t)data[i - 1] << ((width - i) * 8));
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

class simple_ebml_element {
public:
	simple_vint id;
	simple_vint size;
	uint8_t* data;
};

int main(int argc, char** argv){
	int len, mask;
	uint8_t buffer[BUFSIZE];

	int total_bytes = 0;
	int from_bytes = 0;
	int to_bytes = 0;

	bool verbose = false;

	simple_vint* e_id; 
	simple_vint* e_size; 

	for(int i = 0; i < argc; i++){
		if((std::strcmp(argv[i], "-f") == 0) && argc > i){
                        from_bytes = std::stoi(argv[i + 1]);
                }else if((std::strcmp(argv[i], "-t") == 0) && argc > i){
                        to_bytes = std::stoi(argv[i + 1]);
                }else if((std::strcmp(argv[i], "-l") == 0) && argc > i){
                        to_bytes = from_bytes + std::stoi(argv[i + 1]);
                }else if(std::strcmp(argv[i], "-v") == 0){
                        verbose = true;
                }
	}

	if((len = read(STDIN_FILENO, buffer, BUFSIZE)) == -1){
		std::cout << "Uh oh, read error!\n";
		return 1;
	}

	for(int i = 0; i < len;){
		if(to_bytes > 0 && total_bytes + i >= to_bytes){
			break;
		}

		if(verbose){
			std::bitset<8> sbits(buffer[i]);
			std::cout << "Element Start: " << sbits << std::endl;
			std::cout << "Element Pos: " << std::dec << i << std::endl;
		}

		// Get ID VINT

		e_id = new simple_vint();
		e_id->width = 1;
		mask = 0x80;

		// Find the size of the element id.
		while(!(buffer[i] & mask)){
			mask >>= 1;
			e_id->width += 1;
		}
		if(verbose){
			std::cout << "Id Vint Width: " << (int)e_id->width << std::endl;
			std::cout << "Id Vint Bytes: ";
		}
		for(int j = 0; j < e_id->width; ++j){
			e_id->data[j] = buffer[i + j];
			if(verbose){
				std::bitset<8> bits(e_id->data[j]);
				std::cout << bits;
			}
		}
		i += e_id->width;
		if(verbose){
			std::cout << std::endl;
			std::cout << "Id Hex: 0x";
			for(int j = 0; j < e_id->width; ++j){
				std::cout << std::hex << (int)e_id->data[j];
			}
			std::cout << std::endl;
			std::cout << "Id Value: " << std::dec << e_id->get_int() << std::endl;
		}
		
		// Get SIZE VINT

		e_size = new simple_vint();
		e_size->width = 1;
		mask = 0x80;

		if(verbose){
			std::bitset<8> dbits(buffer[i]);
			std::cout << "Data Size Start: " << dbits << std::endl;
		}

		// Find the size of the element data.
		while(!(buffer[i] & mask)){
			mask >>= 1;
			e_size->width += 1;
		}
		if(verbose)
			std::cout << "Data Size Vint Width: " << (int)e_size->width << std::endl;

		buffer[i] ^= mask;

		if(verbose)
			std::cout << "Data Size Vint Bytes: ";
		for(int j = 0; j < e_size->width; ++j){
			e_size->data[j] = buffer[i + j];
			if(verbose){
				std::bitset<8> bits(e_size->data[j]);
				std::cout << bits;
			}
		}
		i += e_size->width;
		if(verbose){
			std::cout << std::endl;
			std::cout << "Data Size: " << std::dec << e_size->get_int() << std::endl;
		}

		// SPEC LOOKUP AND PRINT

		ebml_element* e = get_element(
			{{e_id->data[0], e_id->data[1], e_id->data[2], e_id->data[3]}},
			e_id->width);

		if(e != 0){
			if(verbose)
				std::cout << "-----------------------------";
			if(e->type == MASTER){
				std::cout << std::endl << e->name;
			}else if(e->type == STRING || e->type == UTF8){
				std::cout << std::endl << e->name << ": ";
				for(int j = 0, size = e_size->get_int(); j < size; ++j){
					std::cout << buffer[i + j];
				}
				i += e_size->get_int();
			}else{
				std::cout << std::endl << e->name << ": ";
				for(int j = 0, size = e_size->get_int(); j < size; ++j){
					std::cout << std::hex << (int)buffer[i + j];
				}
				i += e_size->get_int();
			}
			if(verbose)
				std::cout << "\n-----------------------------\n";
		}

		delete e_id;
		delete e_size;
	}

	total_bytes += len;

	return 0;
}

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <bitset>
#include <vector>
#include <array>

#include <unistd.h>

#define BUFSIZE 8192 // 8KB

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

	int64_t get_int(){
		return static_cast<int64_t>(this->get_uint());
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
class basic_log{};
template <class T> basic_log& operator<< (basic_log& l, const T& x){
	std::cout << x;
	return l;
}
basic_log& operator<< (basic_log& l, manip manipulator){
	std::cout << manipulator;
	return l;
}
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
basic_log log;
verbose_log vlog;

int main(int argc, char** argv){
	int len, mask;
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
					std::cout << "Uh oh, could not read all the data!\n";
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
				}else if(e->type == UINT){
					simple_vint data;
					data.width = 0;
					for(int i = 0; i < data_len; ++i){
						data.data[i] = buffer[i];
						data.width++;
					}
					std::cout << std::dec << data.get_uint();
				}else if(e->type == INT){
					simple_vint data;
					data.width = 0;
					for(int i = 0; i < data_len; ++i){
						data.data[i] = buffer[i];
						data.width++;
					}
					std::cout << std::dec << data.get_int();
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
			std::cout << "UNKNOWN ELEMENT: ";
		}
		vlog << "--------------------------------------------------------" << std::endl;
	}


/*
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
			std::cout << "Id Value: " << std::dec << e_id->get_uint() << std::endl;
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
			std::cout << "Data Size: " << std::dec << e_size->get_uint() << std::endl;
		}

		// SPEC LOOKUP AND GET DATA

		ebml_element* e = get_element(
			{{e_id->data[0], e_id->data[1], e_id->data[2], e_id->data[3]}},
			e_id->width);

		if(e != 0){
			if(verbose)
				std::cout << "-----------------------------";
			if(e->type == MASTER){
				std::cout << std::endl << '*' << e->name;
			}else if(e->type == STRING || e->type == UTF8){
				std::cout << std::endl << e->name << ": ";
				for(int j = 0, size = e_size->get_uint(); j < size; ++j){
					std::cout << buffer[i + j];
				}
				i += e_size->get_uint();
			}else if(e->type == BINARY){
				std::cout << std::endl << e->name << ": ";
				for(int j = 0, size = e_size->get_uint(); j < 32 && j < size; ++j){
					std::cout << std::hex << (int)buffer[i + j];
				}
				std::cout << "...";
				i += e_size->get_uint();
			}else if(e->type == UINT){
				e_data = new simple_vint();
				e_data->width = 0;
				for(int j = 0, size = e_size->get_uint(); j < size; ++j){
					e_data->data[j] = buffer[i + j];
					e_data->width += 1;
				}
				std::cout << std::endl << e->name << ": " << std::dec << e_data->get_uint();
				delete e_data;
				i += e_size->get_uint();
			}else if(e->type == INT){
				e_data = new simple_vint();
				e_data->width = 0;
				for(int j = 0, size = e_size->get_uint(); j < size; ++j){
					e_data->data[j] = buffer[i + j];
					e_data->width += 1;
				}
				std::cout << std::endl << e->name << ": " << std::dec << e_data->get_int();
				delete e_data;
				i += e_size->get_uint();
			}else{
				std::cout << std::endl << e->name << ": ";
				for(int j = 0, size = e_size->get_uint(); j < size; ++j){
					std::cout << std::hex << (int)buffer[i + j];
				}
				i += e_size->get_uint();
			}
			if(verbose)
				std::cout << "\n-----------------------------\n";
		}

		delete e_id;
		delete e_size;
	}

	total_bytes += len;
*/
	return 0;
}

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <bitset>

#include <unistd.h>

#define BUFSIZE 8192

enum VintState {
	GET_VINT_WIDTH,
	GET_VINT_DATA
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

	simple_vint* e_id; 
	simple_vint* e_size; 

	for(int i = 0; i < argc; i++){
		if((std::strcmp(argv[i], "-f") == 0) && argc > i){
                        from_bytes = std::stoi(argv[i + 1]);
                }else if((std::strcmp(argv[i], "-t") == 0) && argc > i){
                        to_bytes = std::stoi(argv[i + 1]);
                }else if((std::strcmp(argv[i], "-l") == 0) && argc > i){
                        to_bytes = from_bytes + std::stoi(argv[i + 1]);
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

		std::bitset<8> sbits(buffer[i]);
		std::cout << "Element Start: " << sbits << std::endl;

		e_id = new simple_vint();
		e_id->width = 1;
		mask = 0x80;

		// Find the size of the element id.
		while(!(buffer[i] & mask)){
			mask >>= 1;
			e_id->width += 1;
		}
		std::cout << "Id Vint Width: " << (int)e_id->width << std::endl;

		// Get rid of "vint marker"
		//buffer[i] = 0;
		//buffer[i] ^= mask;

		//vint->data[0] = buffer[i];
		//vint->width = 1;
		//std::cout << "Value: " << std::dec << vint->get_int() << std::endl;

		std::cout << "Id Vint Bytes: ";
		for(int j = 0; j < e_id->width; ++j){
			e_id->data[j] = buffer[i + j];
			std::bitset<8> bits(e_id->data[j]);
			std::cout << bits;
		}
		i += e_id->width;
		std::cout << std::endl;

		std::cout << "Id Hex: 0x";
		for(int j = 0; j < e_id->width; ++j){
			std::cout << std::hex << (int)e_id->data[j];
		}
		std::cout << std::endl;

		std::cout << "Id Value: " << std::dec << e_id->get_int() << std::endl;
		

		e_size = new simple_vint();
		e_size->width = 1;
		mask = 0x80;

		std::bitset<8> dbits(buffer[i]);
		std::cout << "Data Size Start: " << dbits << std::endl;

		// Find the size of the element data.
		while(!(buffer[i] & mask)){
			mask >>= 1;
			e_size->width += 1;
		}
		std::cout << "Data Size Vint Width: " << (int)e_size->width << std::endl;

		buffer[i] ^= mask;

		std::cout << "Data Size Vint Bytes: ";
		for(int j = 0; j < e_size->width; ++j){
			e_size->data[j] = buffer[i + j];
			std::bitset<8> bits(e_size->data[j]);
			std::cout << bits;
		}
		i += e_size->width;
		std::cout << std::endl;

		std::cout << "Data Size: " << std::dec << e_size->get_int() << std::endl;

		// 0x1a45dfa3
		if(e_id->get_int() == 440786851){
			std::cout << "-----------------------------\n";
			std::cout << "\tEBML HEADER\n";
			std::cout << "-----------------------------\n";
		// 0x18538067
		}else if(e_id->get_int() == 408125543){
			std::cout << "-----------------------------\n";
			std::cout << "\tSEGMENT\n";
			std::cout << "-----------------------------\n";
		// 0x114d9b74
		}else if(e_id->get_int() == 290298740){
			std::cout << "-----------------------------\n";
			std::cout << "\tSEEK INFO\n";
			std::cout << "-----------------------------\n";
		// 0x4dbb
		}else if(e_id->get_int() == 19899){
                        std::cout << "-----------------------------\n";
                        std::cout << "\tSEEK\n";
                        std::cout << "-----------------------------\n";
		// 0xec
                }else if(e_id->get_int() == 236){
                        std::cout << "-----------------------------\n";
                        std::cout << "\tVOID\n";
                        std::cout << "-----------------------------\n";
			i += e_size->get_int();
		// 0x1549a966
                }else if(e_id->get_int() == 357149030){
                        std::cout << "-----------------------------\n";
                        std::cout << "\tSEGMENT INFO\n";
                        std::cout << "-----------------------------\n";
		// 0x1654ae6b
                }else if(e_id->get_int() == 374648427){
                        std::cout << "-----------------------------\n";
                        std::cout << "\tTRACKS\n";
                        std::cout << "-----------------------------\n";
		// 0xae
                }else if(e_id->get_int() == 174){
                        std::cout << "-----------------------------\n";
                        std::cout << "\tTRACK ENTRY\n";
                        std::cout << "-----------------------------\n";
		// 0xe0
                }else if(e_id->get_int() == 224){
                        std::cout << "-----------------------------\n";
                        std::cout << "\tTRACK VIDEO\n";
                        std::cout << "-----------------------------\n";
                // Other
                }else{
			std::cout << "Data (bytes): " << std::endl;
			std::cout << "-----------------------------\n";
			for(int j = 0, size = e_size->get_int(); j < size; ++j){
				std::cout << "Byte: " << (int)buffer[i + j] << std::endl;
			}
			i += e_size->get_int();
			std::cout << "-----------------------------\n";
		}

		delete e_id;
		delete e_size;
	}

	total_bytes += len;

	return 0;
}

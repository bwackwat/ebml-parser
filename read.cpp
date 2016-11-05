#include <iostream>
#include <cstring>
#include <cstdlib>
#include <bitset>

#include <unistd.h>

#define BUFSIZE 8192

int main(int argc, char** argv){
	int len;
	uint8_t buffer[BUFSIZE];

	int total_bytes = 0;
	int from_bytes = 0;
	int to_bytes = 0;

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

	std::cout << "Read " << len << " bytes." << std::endl;

	for(int i = 0; i < len; ++i){
		if(to_bytes > 0 && total_bytes + i >= to_bytes){
			break;
		}

		std::bitset<8> bits(buffer[i]);
		std::cout << bits << std::endl;
		std::cout << std::hex << (int)buffer[i] << std::endl;
		std::cout << std::dec << (int)buffer[i] << std::endl;
		std::cout << std::endl;
	}

	total_bytes += len;

	return 0;
}

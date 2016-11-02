#!/bin/bash

clang++ -std=c++11 -g -O3 -Wall -pedantic \
./read.cpp -o read

clang++ -std=c++11 -g -O3 -Wall -pedantic \
./main.cpp -o ebml-parser

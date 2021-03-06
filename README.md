# ebml-parser

This is a simple, yet complete EBML parser.
```
[/bin/bash]# ./build.sh
[/bin/bash]# ./ebml-parser < test.webm | less
```

## Features

* Supports all EBML elements as listed in https://www.matroska.org/technical/specs/index.html
* Does not validate mandatory elements, multiple elements, ranges, or WebM elements.
* Parses incrementally as EBML elements are found.

### Command Line

* ```-v``` for verbose (bitwise) output.

## TODO

* Parse Block lacing bits.
* Reading 1KB batches is likely more efficient; a research topic.

## Suggested Use / Extras

* You can get a test WebM from http://video.webmfiles.org/big-buck-bunny_trailer.webm.
  * I like to split it with ```mkvmerge --split 200ms test.webm -o split```.
* You can test EBML files with https://mkvtoolnix.download/doc/mkvinfo.html.
  * ```mkvinfo -z -v test.webm | less```
* You can also test EBML files with https://www.matroska.org/downloads/mkvalidator.html.
  * ```ebml_validator -M test.webm | less```
* First ```./build.sh``` the program.
  * Then run it: ```./ebml-parser -v < test.webm | less```
* Finally, the ```spec-parser``` folder contains some whacky python which parses EBML elements from the Matroska technical specification linked above, and basically creates ```spec.cpp```.

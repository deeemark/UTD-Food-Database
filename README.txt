UTD Food Database system
A lightweight page-based C++ Database Management Systems with a buffer pool and a bloom filter. Successfully test on dataset from 10-1 million.
Features
* Persistent on-disk format(but set up to erase the before every run for testing purposes)
* Buffer Pool with FIFO replacement
* Page pin/unpin, dirty-page tracking
* Bloom filter integration in leaf node
* Searching
* Average access time/ bloom filter performance testing
* Configurable tree order + page size

Configurable Parameters
ORDER: controls max keys per node (MAX_KEYS * 2 & MAX_CHILDREN = 2 * ORDER +1)
(default 15 located in bPlusTree.h)
PAGE_SIZE: size of node page on disk(default 16k located in FileDiskManager.h)
BUFFER_POOL_SIZE: number of frames in memory(default 10 located in main.cpp)
BLOOM_BITS: Bloom filter size per leaf (default 256 located in BloomFilter.h)

Expected input and output:
input: one of the 4 csv files in the main folder
output: tree_data.bin which stores pages in byte form.

Dependencies and assumption: None besides a up to date C++ version

Usage
in the terminal navigate to your folder and
1: make clean
2: make
3: make run
3: Enter one of the file names in the folder with the executable (named according to size):
which are 100items.csv(1 hundred items), 1000items.csv(1 thousand items), 10000items.csv (10 thousand items), 
1Mitems.csv (1 million items capitalization is important)
4:Navigate menu by typing choice and entering.
tested on the engx server using no machine and on windows using mysys2 mingw64

Future Work
* Secondary indexing
* Object heap and better serialization for expanded categories
* WAL logging
Developed by:DeMarkus Taylor, Nicolas Lee, Stephen Chang, Subhayan Basu
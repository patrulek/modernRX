## Changelog

### v0.2.2 - 09.09.2023:

Dataset generation simplification by adding padding to the end of the dataset.

Improvements:
* remove some unnecessary code to handle remainder dataset items (now 4-batch function will never leave remainder items)
* this will simplify JIT compiling code (no need to code two compiler versions for superscalar programs) 

Bug fixes:
* fix Blake2bRandom and Superscalar functions to use noexcept

### v0.2.1 - 09.09.2023:

Dataset items batching and caching reciprocals in superscalar programs.
All optimization decisions made in this version:

* add batching to dataset generation with batch size of 4 items (this should be nicely AVX2-able when JIT compiler is ready)
* add AVX2 instructions for Dataset generation
* add reciprocals caching in superscalar programs to not calculate them every time item is generated
* add assumption for reciprocal function that it will always be called with valid argument (thats guaranteed by RandomX design)
* change std::copy to std::memcpy as it seems that std::memcpy is willing to produce better code

Bug fixes:
* fix Argon2d input size assumption (to satisfy tests)

Improvements:
* add another Argon2d test case

Lessons learned:
* std::memcpy may produce better code than std::copy

### v0.2.0 - 08.09.2023:

Dataset multithreading and AVX2 support for Blake2b and Argon2d.
All optimization decisions made in this version:

* add multithreading to dataset generation; work is easily parallelizable and scales well with number of threads
* add avx2 instructions to blake2b and argon2d
* set avx2 instruction set as minimum requirement for the project
* implement some assumptions about blake2b usage:
  * key is never used, thus was removed
  * compression loop was unrolled and simplified because it's never used with more than 2 blocks
  * second counter is never used, thus was removed
  * function is used with some fixed input and output sizes, and those assumptions were used as hints to compiler 
* implement some assumptions about argon2d usage:
  * single lane is always used, thus lane-loop and some instructions were removed, as they are fixed now
  * algorithm parameters are fixed, thus some structs and function arguments were removed
  * function is used with some fixed input and output sizes, and those assumptions were used as hints to compiler

Lessons learned:
* sometimes C-array may be faster than std::array (Argon2d example)
* using native SIMD types may be faster than equally sized std::array (easier for compiler to optimize)
* unrolling loops may slow down code (Argon2d example; i guess that unrolled version grew too much and compiler denied to inline calls to avx2 wrapper functions)

### 0.1.1 - 07.09.2023:

Some cleanup and improvements:

* increase benchmark time from 10 to 60 seconds
* change some benchmarks to calculate hashes per second instead of megabytes per second and added benchmark for measuring efficiency of the algorithm
* add some test cases to increase code coverage (not measured yet)
* change alias for XMM registers in sse.hpp to use __m128d instead of std::array
* clean up some code in utils
* clean up some code in blake2b and argon2d
* clean up projects properties

### 0.1.0 - 03.09.2023:

Reference implementation of the project.
Later optimization and improvements will base on this version.
All of the code was meant to be tested, documented and followed coding guidelines.

### 0.0.1 - 10.08.2023:

Initial implementation of the project.
RandomX hashes are calculated correctly, but no optimizatons are included.
Most of the code is tested, documented and follows coding guidelines.

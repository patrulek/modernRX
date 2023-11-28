## modernRX - Changelog

### v0.7.2 - 28.11.2023:

Hash calculation optimization with thread affinity and scratchpad hash&fill.
All optimization decisions made in this version:
* add scratchpad prefetching with aes hash and fill
* enforce alignment on some variables used in aes hash and fill

### v0.7.1 - 28.11.2023:

Hash calculation optimization with thread affinity and scratchpad hash&fill.
All optimization decisions made in this version:
* add thread affinity (to prevent threads from migrating between cores and losing cache)
* combine hash and fill scratchpad (to increase ILP)
* allocate all VM's scratchpad memory in single chunk (potentially better L3 cache hit ratio)

Improvements:
* remove multiple loop finalization points in jit code buffer
* remove deprecated jit program argument
* move dataset offset calculation to jit program
* small jit code buffer tweaks
* general code improvements

### v0.7.0 - 17.11.2023:

Hash calculation optimization with multi-threading.
All optimization decisions made in this version:
* add multithreaded hash calculation

Other:
* adjust tests and fuzzing for new API

### v0.6.3 - 16.11.2023:

Hash calculation optimization (slightly faster JIT compilation).
All optimization decisions made in this version:
* jit compilation speed improved:
  * single initialization of code buffer for static part of code program (prologue and epilogue, that's equal for all programs)
  * faster bytecode compiler (less instructions)
* hand-write assembly for program context initialization (C++ code moved to hand-written assembly that align better with JIT-compiled programs)
* improve loop begin and end of a program slightly (mostly as a result of hand-writing program context initialization)

### v0.6.2 - 11.11.2023:

Hash calculation optimization ("insecure" mode, fewer allocations).
All optimization decisions made in this version:
* remove unnecessary data allocations
* insecure JIT mode (don't protect and reuse allocated virtual memory buffer for JIT code)
* add machine code injection directly into assembly context instead of using functions to generate code (this makes code less readable though)
* assembly context uses now HeapArray instead of std::vector for code buffer 

Improvements:
* HeapArray improvements to match part of std::vector api

### v0.6.1 - 09.11.2023:

Hand-written assembly around jitted programs.
All optimization decisions made in this version:
* hand-write assembly around JIT-compiled random programs 
* change assembler's labels type (string -> int) and limit to 512 labels

Improvements:
* rename and refactor interpreter/jitcompiler
* JIT code buffer little api change

### v0.6.0 - 08.11.2023:

Random programs JIT-compiler.
All optimization decisions made in this version:
* add JIT-compilation of random programs in place of interpreted execution 

### v0.5.1 - 05.11.2023:

Hash calculation optimization (interpreter improvement).
All optimization decisions made in this version:
* change switch-based to subroutine-based interpreter
* prefetch dataset items
* precalculate some instruction values

### v0.5.0 - 30.10.2023:

Hash calculation optimization with AES instructions.
All optimization decisions made in this version:
* replace software AES calculation with hardware AES instructions

Improvements:
* documentation corrections

### v0.4.0 - 29.10.2023:

Maintenance release.

Improvements:
* update benchmarks for RandomX 1.2.1 version
* unify [MB/s] benchmarks to make it clear 1MB=1'048'576B
* add benchmarks for additional setups
* add project that utilizes libfuzzer
* add project that utilizes profile-guided optimization
* use AddressSanitizer in tests
* add more test cases
* add code coverage report
* refactor and cleanup code
* add CPU capabilities check

Bug fixes:
* fix bugs reported by AddressSanitizer
* fix bugs in superscalar generator

### 0.1.3 - 29.10.2023:

Changes in current version:

* fix bug in reciprocal calculation for random program
* fix hasher bug in key append
* fix bugs in superscalar generator
* unify [MB/s] benchmarks to make it clear 1MB=1'048'576B
* update benchmarks for RandomX 1.2.1 version
* add new test case
* cleanup code

### v0.3.12 - 23.10.2023:

Blake2b optimizations.
All optimization decisions made in this version:
* reorder some instructions to reduce chain dependency

### v0.3.11 - 22.10.2023:

Dataset generation and JIT-compiler general improvements.
All optimization decisions made in this version:
* split dataset generation tasks into smaller jobs (this should improve performance a bit when other workloads are present on single core)
* replace all data addressing that used 32-bit displacement with 8-bit displacement (lesser total code size)

Improvements:
* make JIT-compiler compliant with x64 Windows calling convention
* add stack alignment to JIT-compiler
* add small instruction improvements to JIT-compiler
* make JIT compilation code more generic
* add JIT program data section alignment
* make JIT-compiler data section size configurable (data pointers are lazy evaluated before compilation instead of a priori)

### v0.3.10 - 20.10.2023:

Argon2d optimizations.
All optimization decisions made in this version:
* add prefetching of referenced blocks in Argon2d
* reorder some instructions in Argon2d

Improvements:
* simplify Argon2d functions for (already existed) single lane assumption
* update Argon2d documentation

### v0.3.9 - 16.10.2023:

JIT-compiler optimizations.
All optimization decisions made in this version:
* optimize IMUL_R/IMUL_RCP by changing algorithm (more instructions, but less latency)
* reorder some instructions

Lessons learned:
* profiling showed that after replacing microcoded VPHADDD instruction with simpler ones, fetch-to-retire latency decreased and IPC increased, slightly improving overall performance
* microcoded instructions may stall frontend which could be the case here

### v0.3.8 - 08.10.2023:

JIT-compiler optimizations.
All optimization decisions made in this version:
* compile all superscalar programs as single function
* code around superscalar programs (dataset item generation function) was optimized with hand-written assembly to align better with superscalar programs

Lessons learned:
* writing hand-written assembly is hard and time-consuming
* debugging and profiling JIT-compiled code is even harder

### v0.3.7 - 08.10.2023:

Dataset generation optimizations.
All optimization decisions made in this version:
* add heaparray container for fixed-sized std::vector replacement
* optimize blake2b/argon2d instruction order

Bug fixes:
* fix interpreter bug with reciprocals

### v0.3.6 - 03.10.2023:

JIT-compiler optimizations.
All optimization decisions made in this version:
* remove generation of some load (vmovdqa) instructions and use memory operand directly (less instructions)

### v0.3.5 - 02.10.2023:

Dataset items generation simplification:
* replace some scalar code with vectorized version for consistency and simplicity

This change may have negative impact on performance for now, but it's hard to estimate real impact after making whole function hand-written assembly.
It should be easier to hand-write assembly with this change, but in case of replaced scalar code being faster, it will be reverted.

### v0.3.4 - 30.09.2023:

JIT-compiler optimizations.
All optimization decisions made in this version:
* move away immediate values to data "section" (less instructions - single vmovdqa instead of mov, vmovq and vpbroadcastq)

Improvements:
* documentation corrections in tests

### v0.3.3 - 30.09.2023:

Dataset items initialization optimizations.
All optimization decisions made in this version:
* replace multiplication with addition in dataset item initialization (less instructions; this may not have direct effect now, but it should be slightly better when whole dataset generation function will be written in assembly)

### v0.3.2 - 29.09.2023:

JIT-compiler optimizations.
All optimization decisions made in this version:
* optimize IMUL/IMUL_RCP by changing algorithm (less instructions)
* optimize ISMULH by removing another vpxor instruction (it's done once at program initialization)

### v0.3.1 - 29.09.2023:

JIT-compiler optimizations.
All optimization decisions made in this version:
* optimize IMULH/ISMULH by moving fixed 32-bit mask to register once at program initialzation
* optimize ISMULH by removing unnecessary vpxor instruction

### v0.3.0 - 28.09.2023:

Superscalar program JIT compilation.
All optimization decisions made in this version:
* add JIT compilation of superscalar programs using AVX2 instructions
* add cache item prefetching in dataset generation

Bug fixes:
* fix bug in reciprocal generation and calculation for Superscalar program

Improvements:
* rename superscalar program related structures with Superscalar prefix to not conflict with random program related structures
* correct original randomx dataset benchmark result (it was calculated incorrectly and is slightly higher now)
* documentation corrections

Lessons learned:
* x86_64 instruction encoding is pretty messed up

### v0.1.2 - 28.09.2023:

Changes in current version:

* fix bug in reciprocal generation and calculation for Superscalar program
* fix copy bug in hasher reset
* correct original randomx dataset benchmark result (it was calculated incorrectly and is slightly higher now)
* add few new testcases
* rename superscalar program related structures with Superscalar prefix to not conflict with random program related structures
* documentation corrections

### v0.2.3 - 11.09.2023:

Superscalar program execution optimizations.
All optimization decisions made in this version:
* use batch of 4 dataset items in single superscalar program execution (previously superscalar program was called for each item separately)
* add AVX2 instructions to superscalar program execution

Improvements:
* add new assertions in dataset generation test

Bug fixes:
* fix project filters

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

Bug fixes:
* fix copy bug in hasher reset

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

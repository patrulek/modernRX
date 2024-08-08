# modernRX - Modern C++ RandomX Implementation

## Overview

This project aims to provide minimal implementation of [RandomX v1 algorithm](https://github.com/tevador/RandomX) using modern C++ language.

Current state of this project does not provide sensible performance to use in mining, but you can still use this repository for educational purposes (or whatever that will make sense for you).

## Features

* Self-contained: no external dependencies and most of the code is written from scratch based on RandomX specs, design and source code.
* Modern: written with most recent C++ features in mind (C\++20 and C\++23).
* Understandable: code is much more concise, readable and better documented than original RandomX implementation (at least i believe it is).
* Minimal: as little code as possible to be able to run on most recent Intel/AMD CPUs (in exchange for portability).

## Roadmap

* [x] (10.08.2023) Provide minimal implementation that yield correct RandomX hashes.
* [x] (03.09.2023) Polish tests and documentation, add benchmarks.
* [ ] Optimize dataset generation with multithreading and hardware specific instructions.
* [ ] Optimize dataset generation with JIT compiler for superscalar programs.
* [ ] Optimize hash calculation with hardware specific instructions.
* [ ] Optimize hash calculation with JIT compiler for random programs.
* [ ] Optimize hash calculation with multithreading.
* [ ] Experiment with further JIT optimizations for faster hash calculation.
* [ ] Experiment with system and architecture specific optimizations (Huge Pages, MSR etc.) for faster hash calculation.
* [ ] Port library to Unix-based systems.
* [ ] Implement RandomX light mode.
* [ ] Implement RandomX GPU mode.

## Build and run

To build this repository you should download the most recent Visual Studio version (at least 17.10) with C++ tools.

To run tests open solution, set `tests` project as the startup one and click "run". Please be aware that tests can run up to an hour, because of unoptimized dataset generation in this branch.
Sample output:

```console
[ 0] Blake2b::hash                            ... Passed (<1ms)
[ 1] Argon2d::Blake2b::hash                   ... Passed (<1ms)
[ 2] Argon2d::fillMemory                      ... Passed (1.170s)
[ 3] AesGenerator1R::fill                     ... Passed (<1ms)
[ 4] AesGenerator4R::fill                     ... Passed (<1ms)
[ 5] AesHash1R                                ... Passed (<1ms)
[ 6] Blake2brandom::get                       ... Passed (<1ms)
[ 7] Reciprocal                               ... Passed (<1ms)
[ 8] Superscalar::generate                    ... Passed (0.002s)
[ 9] Dataset::generate                        ... Passed (348.839s)
[10] Hasher::run                              ... Passed (412.079s)
```

### Portability

No plans for support multi OSes, platforms or architectures in the nearest future and the only guaranteed environment is the one i develop on, which is:

* System: Windows 11 Home
* CPU: Zen 4 (Ryzen 7840HS)

But it should work with Windows 7 and higher and any 64-bit little-endian CPU.

## Quick start

This repository is meant to be build as a library that can be linked into other programs, specifically Monero miner programs.

```c++
#include "hasher.hpp"

// Initialize hasher somewhere in your code.
modernRX::Hasher hasher{};

void calcHash() {
	// Miner's responsibility to get proper data from blockchain node and setup job parameters.
	// For tests these values are hardcoded.
	auto [key, input] { miner.getHashingDataFromNode() };	

	// calculate hash
	hasher.reset(key); // Resets hasher with new key.
	auto hash{ hasher.run(input) }; // Calculates hash for input data.

	// Miner's responsibility to do something with calculated hash.
	miner.doSomethingWithHash(hash);
}
```

## Benchmarks

Benchmarks were performed on AMD Ryzen 7 7840HS (Radeon 780M Graphics) with 32GB of RAM (4x8GB, dual-channel, 6400 MT/s, DDR5) and Windows 11 Home.
Code was compiled with Microsoft Visual Studio 17.10.5 version.

CPU frequency turbo boost was disabled (3.8GHz base frequency).

Benchmarks compare modernRX implementation with fully optimized RandomX implementation and with RandomX implementation that match optimizations available in current modernRX version.

|                                      | Blake2b [H/s] | Blake2bLong [H/s] | Argon2d [MB/s] | Aes1R [MB/s] | Aes4R [MB/s] | AesHash1R [H/s] | Superscalar [Prog/s] | Dataset [MB/s] | Hash [H/s] | Efficiency [H/Watt/s] |
| ------------------------------------ | :-----------: | :---------------: | :------------: | :----------: | :----------: | :-------------: | :------------------: | :------------: | :--------: | :-------------------: |
| RandomX-1.2.1 (102f8acf)             |    **4.098M** |       **131.76K** |      **884.9** |  **56978.1** |  **13764.3** |       **28528** |                 5921 |     **~932.4** |   **3753** |            **~87.27** |
| RandomX-1.2.1 (102f8acf)<sup>1</sup> |        4.098M |           131.76K |          426.0 |       2844.8 |        639.6 |            1368 |                 5921 |            3.4 |       25.7 |                 ~1.51 |
| modernRX 0.1.4 (reference)           |        3.296M |           107.71K |          465.5 |       3326.9 |        870.5 |            1738 |            **11754** |            7.0 |       40.4 |                 ~2.29 |

 <sup>1)</sup> no avx argon2d, interpreted mode, software AES mode, small pages mode, no batch, single-threaded, full memory mode

 Original RandomX provides benchmark only for calculating final hashes. All other values were estimated (based on information benchmark provides) or some custom benchmarks were written on top of original RandomX implementation, thus values may not be 100% accurate.

 Benchmarks description:

* Blake2b - calculating 64-byte blake2b hash for 64 bytes of input data.
* Blake2bLong - calculating 1024-byte blake2b hash for 72 bytes of input data.
* Argon2d - filling 256 MB of memory.
* Aes1R - generating 2MB of output data with 64 bytes of input data.
* Aes4R - generating 2176 bytes of output data with 64 bytes of input data.
* AesHash1R - calculating 64-byte aes hash for 2MB of input data.
* Superscalar - generating superscalar program.
* Dataset - generating >2GB of dataset.
* Hash - calculating final RandomX hash.
* Efficiency - calculating final RandomX hash per watt per second. Power consumption was measured by wattmeter. 

Units:
* H/s - hashes per second
* MB/s - megabytes (1'048'576 bytes) per second
* Prog/s - programs per second
* H/Watt/s - hashes per watt per second

## Coding guidelines

Code found in this repository follows guidelines listed below:

* Use most recent C++ features wherever possible (mostly for educational purposes).
* Do not use external dependencies (only STL and compiler intrinsics allowed).
* Do not allow compiler warnings in master branch (at least /W3 level).
* Every optimization decision must be thoroughly explained and documented (mostly for educational purposes).
* Avoid using preprocessor and macros. Every exception must be documented.
* Describe all headers at the beginning of file, right before imports.
* Document all functions and types in headers. If function is declared in .cpp prefer documentation close to the definition, not declaration.
* Use forward declarations wherever possible.
* If output parameters are chosen over returned one, they must be the left-most arguments in function.
* Use consteval/constexpr/const wherever possible.
* Constexpr variables used only within single functions should be defined in that function.
* Use inline for constexpr variables in headers. All exceptions from this rule must be documented.
* Prefer spans and ranges instead of arrays and vectors as function parameters.
* Use `[[nodiscard]]`, `noexcept` and `explicit` wherever possible.
* Do not use `[[unlikely]]` and `[[likely]]`.
* Functions with single boolean parameter should be splitted into two separate functions or implemented as a single template function with boolean template parameter.
* It is allowed for inner functions to be stored as lambdas within outer functions that defines that lambda, if there is no other valid usage of such lambda.
* Preallocate data buffers wherever possible.
* Prefer reusing memory with output parameters instead of returned ones wherever suitable.
* Prefer using unnamed namespaces over static functions and private methods that does not require access to class field.
* All functions that belongs to unnamed namespaces must be declared at the beginning and defined at the end of a .cpp file.

## Versioning

Project follows [zero-based versioning](https://0ver.org/) with several specific versions:

* `0.0.1` - initial implementation
* `0.1.0` - reference implementation (contains polished documentation and tests + benchmarks; reference point to further tweaks and optimizations)
* `0.1.x` - reference implementation++ (contains bugfixes, documentation corrections, coding standard updates etc.)
* `0.2+.y` - optimized implementation (contains subsequent optimizations)

## Changelog

* **v0.1.4 - 08.08.2024:** upgrade code to MSVC 17.10.5, add benchmark for new setup
* **v0.1.3 - 29.10.2023:** bugfixes, benchmarks corrections, code cleanup
* **v0.1.2 - 28.09.2023:** bugfixes, renaming, documentation updates
* **v0.1.1 - 07.09.2023:** cleanup some code and projects properties
* **v0.1.0 - 03.09.2023:** reference implementation
* **v0.0.1 - 10.08.2023:** initial implementation

For more details see [CHANGELOG.md](CHANGELOG.md).

### Code statistics (via [gocloc](https://github.com/hhatto/gocloc))

```console
$> gocloc /exclude-ext "xml,json,txt,exp" /not-match-d "3rdparty/*|x64/*|assets/*" .
-------------------------------------------------------------------------------
Language                     files          blank        comment           code
-------------------------------------------------------------------------------
C++                             14            477            263           2271
C++ Header                      22            202            265           1001
Markdown                         2             63              0            184
-------------------------------------------------------------------------------
TOTAL                           38            742            528           3456
-------------------------------------------------------------------------------
```

## Support

If you like this project and would like to support further development you could donate some cryptos to one of those addresses:

* **BTC:** `bc1qrlw6kdtnwka2ww6j9cvzugs7sk8zk2ewp3mrd6`
* **ETH/USDT:** `0x3679B13D96DF9291eB98d1308008Be73F8D05b5B`
* **XMR:** `43hGi1uJDmbGE7RUZJoBLa6BnCDtT3tNdb7v7VMQbheohso81CoUMeJTA7wcHt9Xi27Cw8tPGPex55mpoT3q46MGMa7ca6m`

All donations are voluntary and i will be grateful for every single satoshi.

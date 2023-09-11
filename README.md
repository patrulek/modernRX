# modernRX - Modern C++ RandomX Implementation

## Overview

This project aims to provide minimal implementation of [RandomX algorithm](https://github.com/tevador/RandomX) using modern C++ language.

Current state of this project does not provide sensible performance to use in mining, but you can still use this repository for educational purposes (or whatever that will make sense for you).

## Features

* Self-contained: no external dependencies and most of the code is written from scratch based on RandomX specs, design and source code.
* Modern: written with most recent C++ features in mind (C\++20 and C\++23).
* Understandable: code is much more concise, readable and better documented than original RandomX implementation (at least i believe it is).
* Minimal: as little code as possible to be able to run on most recent Intel/AMD CPUs (in exchange for portability).

## Roadmap

* [x] (10.08.2023) Provide minimal implementation that yield correct RandomX hashes.
* [x] (03.09.2023) Polish tests and documentation, add benchmarks.
* [x] (08.09.2023) Optimize dataset generation with multithreading and hardware specific instructions.
* [ ] Optimize dataset generation with JIT compiler for superscalar programs.
* [ ] Optimize hash calculation with hardware specific instructions.
* [ ] Optimize hash calculation with JIT compiler for random programs.
* [ ] Optimize hash calculation with multithreading.
* [ ] Experiment with further JIT optimizations for faster hash calculation.
* [ ] Experiment with system and architecture specific optimizations (Huge Pages, MSR etc.) for faster hash calculation.
* [ ] Implement RandomX light mode.
* [ ] Implement RandomX GPU mode.
* [ ] Port library to Unix-based systems.

## Build and run

To build this repository you should download the most recent Visual Studio version (at least 17.7) with C++ tools.

Library requires support for AVX2 instructions. There are no runtime checks so if its crashing, make sure your CPU supports AVX2.

To run tests open solution, set `tests` project as the startup one and click "run".
Sample output:

```console
[ 0] Blake2b::hash                            ... Passed (<1ms)
[ 1] Argon2d::Blake2b::hash                   ... Passed (<1ms)
[ 2] Argon2d::fillMemory                      ... Passed (0.614s)
[ 3] AesGenerator1R::fill                     ... Passed (<1ms)
[ 4] AesGenerator4R::fill                     ... Passed (<1ms)
[ 5] AesHash1R                                ... Passed (<1ms)
[ 6] Blake2brandom::get                       ... Passed (<1ms)
[ 7] Reciprocal                               ... Passed (<1ms)
[ 8] Superscalar::generate                    ... Passed (0.001s)
[ 9] Dataset::generate                        ... Passed (22.705s)
[10] Hasher::run                              ... Passed (22.924s)
```

### Portability

No plans for support multi OSes, platforms or architectures in the nearest future and the only guaranteed environment is the one i develop on, which is:

* System: Windows 11
* CPU: Zen 3 (Ryzen 5800H)

But it should work with Windows 7 and higher and any 64-bit little-endian CPU with AVX2 support.

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

Benchmarks were performed on Ryzen 5800H CPU with 32GB of RAM (Dual-channel, 3200 MT/s) and Windows 11.

CPU frequency turbo boost was disabled (3.2GHz base frequency).

Benchmarks compare modernRX implementation with fully optimized RandomX implementation and with RandomX implementation that match optimizations available in current modernRX version.

|                                | Blake2b [H/s] | Blake2bLong [H/s] | Argon2d [MB/s] | Aes1R [MB/s] | Aes4R [MB/s] | AesHash1R [H/s] | Superscalar [Prog/s] | Dataset [MB/s] | Hash [H/s] | Efficiency [H/Watt/s] |
| ------------------------------ | :-----------: | :---------------: | :------------: | :----------: | :----------: | :-------------: | :------------------: | :------------: | :--------: | :-------------------: |
| RandomX (901f8ef7)             |        3.178M |           102.18K |          912.9 |  **48987.6** |  **12004.5** |       **23510** |                 3997 |     **~731.5** |   **4510** |            **~73.93** |
| RandomX (901f8ef7)<sup>2</sup> |        3.178M |           102.18K |          912.9 |       2412.8 |        548.5 |            1153 |                 3997 |          ~28.8 |       19.9 |                 ~0.71 |
| modernRX 0.2.3                 |		  4.872M |           154.30K |		    957.1 |       2789.8 |        732.6 |            1394 |                 9356 |          113.1 |       26.2 |                 ~0.87 |
| modernRX 0.2.2                 |		  4.893M |           156.04K |		    988.0 |       2789.2 |        742.4 |            1415 |                 9419 |           38.7 |       25.8 |                 ~0.83 |
| modernRX 0.2.1                 |    **4.903M** |       **156.11K** |          973.0 |       2868.4 |        734.8 |            1436 |             **9458** |           36.0 |       25.9 |                 ~0.89 |
| modernRX 0.2.0                 |		  4.902M |           154.70K |      **990.3** |       2893.3 |        751.6 |            1419 |                 9409 |           19.2 |       25.8 |                 ~0.86 |
| RandomX (901f8ef7)<sup>1</sup> |        3.178M |           102.18K |          400.6 |       2412.8 |        548.5 |            1153 |                 3997 |           ~2.1 |       19.9 |                 ~0.71 |
| modernRX 0.1.1 (reference)     |        2.134M |            69.48K |          407.2 |       2877.2 |        735.4 |            1434 |                 8223 |            1.7 |       26.4 |                 ~0.91 |

<sup>1)</sup> no avx argon2d, interpreted mode, software AES mode, small pages mode, no batch, single-threaded, full memory mode

<sup>2)</sup> avx2 argon2d, interpreted mode, software AES mode, small pages mode, no batch, multi-threaded dataset, single-threaded hash, full memory mode

Original RandomX provides benchmark only for calculating final hashes. All other values were estimated (based on information benchmark provides) or some custom benchmarks were written on top of original RandomX implementation, thus values may not be 100% accurate.

Benchmarks description:

* Blake2b - calculating 64-byte blake2b hash for 64 bytes of input data.
* Blake2bLong - calculating 1024-byte blake2b hash for 64 bytes of input data.
* Argon2d - filling 256 MB of memory.
* Aes1R - generating 2MB of output data with 64 bytes of input data.
* Aes4R - generating 2176 bytes of output data with 64 bytes of input data.
* AesHash1R - calculating 64-byte aes hash for 2MB of input data.
* Superscalar - generating superscalar program.
* Dataset - generating >2GB of dataset.
* Hash - calculating final RandomX hash.
* Efficiency - calculating final RandomX hash per watt per second. Power consumption was measured by wattmeter.

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
* If output parameter is chosen over returned one, it must be the first argument in function.
* No more than single output parameter per function.
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

* **v0.2.3 - 09.09.2023:** dataset generation optimizations (item batching and AVX2 support for superscalar program execution)
* **v0.2.2 - 09.09.2023:** dataset generation simplification (add dataset padding)
* **v0.2.1 - 09.09.2023:** dataset generation optimizations (item batching and caching reciprocals)
* **v0.2.0 - 08.09.2023:** dataset generation optimizations (multithreading and AVX2 support for Blake2b and Argon2d)
* **v0.1.1 - 07.09.2023:** cleanup some code and projects properties
* **v0.1.0 - 03.09.2023:** reference implementation
* **v0.0.1 - 10.08.2023:** initial implementation

For more details see [CHANGELOG.md](CHANGELOG.md).

### Code statistics (via [gocloc](https://github.com/hhatto/gocloc))

```console
$> gocloc /exclude-ext xml,json,txt .
-------------------------------------------------------------------------------
Language                     files          blank        comment           code
-------------------------------------------------------------------------------
C++                             14            483            307           2217
C++ Header                      26            284            330           1670
Markdown                         2             73              0            215
-------------------------------------------------------------------------------
TOTAL                           42            840            637           4102
-------------------------------------------------------------------------------
```

## Support

If you like this project and would like to support further development you could donate some cryptos to one of those addresses:

* **BTC:** `bc1qrlw6kdtnwka2ww6j9cvzugs7sk8zk2ewp3mrd6`
* **ETH/USDT:** `0x3679B13D96DF9291eB98d1308008Be73F8D05b5B`
* **XMR:** `43hGi1uJDmbGE7RUZJoBLa6BnCDtT3tNdb7v7VMQbheohso81CoUMeJTA7wcHt9Xi27Cw8tPGPex55mpoT3q46MGMa7ca6m`

All donations are voluntary and i will be grateful for every single satoshi.

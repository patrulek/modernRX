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
* [x] (08.09.2023) Optimize dataset generation with multithreading and hardware specific instructions.
* [x] (28.09.2023) Optimize dataset generation with JIT compiler for superscalar programs.
* [x] (30.10.2023) Optimize hash calculation with hardware specific instructions.
* [x] (08.11.2023) Optimize hash calculation with JIT compiler for random programs.
* [x] (17.11.2023) Optimize hash calculation with multithreading.
* [ ] Experiment with further JIT optimizations for faster hash calculation.
* [ ] Experiment with system and architecture specific optimizations (Huge Pages, MSR etc.) for faster hash calculation.
* [ ] Port library to Unix-based systems.
* [ ] Implement RandomX light mode.
* [ ] Implement RandomX GPU mode.

## Build and run

To build this repository you should download the most recent Visual Studio version (at least 17.7) with C++ tools.

Library requires support for AVX2 and AES instructions. In a case of lacking support, exception will be thrown at runtime.

### Portability

No plans for support multi OSes, platforms or architectures in the nearest future and the only guaranteed environment is the one i develop on, which is:

* System: Windows 11
* CPU: Zen 3 (Ryzen 5800H)

But it should work with Windows 7 and higher and any 64-bit little-endian CPU with AVX2/AES support.

## Quick start

This repository is meant to be build as a library that can be linked into other programs, specifically Monero miner programs.

```c++
#include "modernRX/hasher.hpp"

/*
* This example counts the number of generated hashes.
*/

// Declare hasher somewhere in your code.
std::unique_ptr<modernRX::Hasher> hasher{ nullptr };
std::atomic<uint64_t> counter{ 0 };

// It is advised to initialize it in a place where exception can be handled.
void initialize() {
    try {
        hasher = std::make_unique<modernRX::Hasher>();

        // This will start VirtualMachine worker threads with a callback function that passes calculated hash. 
        hasher->run([&counter](const modernRX::RxHash& hash) {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    } catch (const modernRX::Exception& ex) {
        // Handle exception.
    }
}

void close() {
    // Stop all VirtualMachine worker threads (waits for all of them to finish).
    hasher->stop();
    std::println("Hashes generated: {}", counter.load());
}

void resetJobParameters() {
    // Miner's responsibility to get proper data from blockchain node and setup job parameters.
    // For tests these values are hardcoded.
    auto [key, input] { miner.getHashingDataFromNode() };    

    // Update job parameters.
    hasher->reset(key); // Resets hasher with new key (it will recalculate dataset if new key provided).
    hasher->resetVM(input); // Resets all VirtualMachine worker threads with new input.
}
```

## Tests

To run tests open solution, set `tests` project with `ReleaseAsan` configuration as the startup one and click "run".
Sample output:

```console
[ 0] Blake2b::hash                            ... Passed (<1ms)
[ 1] Argon2d::Blake2b::hash                   ... Passed (<1ms)
[ 2] Argon2d::fillMemory                      ... Passed (20.671s)
[ 3] AesGenerator1R::fill                     ... Passed (<1ms)
[ 4] AesGenerator4R::fill                     ... Passed (<1ms)
[ 5] AesHash1R                                ... Passed (<1ms)
[ 6] Blake2brandom::get                       ... Passed (<1ms)
[ 7] Reciprocal                               ... Passed (<1ms)
[ 8] Superscalar::generate                    ... Passed (0.010s)
[ 9] Dataset::generate                        ... Passed (26.625s)
[10] VirtualMachine::execute                  ... Passed (17.685s)
```

Ideally, tests should be run before every release in `Release` and `Debug` mode with `AddressSanitizer` enabled. `ReleaseAsan` and `DebugAsan` project configurations are provided for this purpose.

### Fuzzing

Repository contains `fuzzer` project that can be used to fuzz this library with `libFuzzer`.
Except of fuzzing to find fatal bugs, it is also used to fuzz against original RandomX implementation to find differences in behavior.

Ideally, fuzzing should be run before every release in `Release` mode with `AddressSanitizer` enabled. 
`ReleaseFuzzer` project configuration is provided for this purpose.

Options used for fuzzing:

```console
-max_len=76 -len_control=0 -rss_limit_mb=8192 -max_total_time=14400 -dict=.dict -artifact_prefix=c:/tmp/ -print_pcs=1 -print_final_stats=1 -report_slow_units=120
```

Be aware that fuzzing is very resource intensive and may take a lot of time to complete.
By default it expect up to 8GB of RAM usage and runs for 4 hours.

### Code coverage

[OpenCppCoverage](https://github.com/OpenCppCoverage/OpenCppCoverage) is used to measure code coverage. To run code coverage download and install latest OpenCppCoverage for Windows, build `tests` project with `Debug` configuration and finally run it with command:

```console
OpenCppCoverage.exe --sources \path\to\modernRX\ -- \path\to\modernRX\x64\Debug\tests.exe
```

Ideally, code coverage should be checked before every release in `Debug` mode.

For a full report see [code coverage report](https://rawcdn.githack.com/patrulek/modernRX/master/assets/covreport/index.html).

## Profile-guided optimization

Repository contains `pgo` project that can be used to generate profile data in `Release` mode. `ReleasePgo` project configuration is provided for this purpose.
In a case of missing `pgort140.dll` just copy it from VisualStudio installation folder to `x64/ReleasePGO` folder.

Generated profiles are used for benchmarking (experimental).

## Benchmarks

Benchmarks were performed on AMD Ryzen 5800H CPU with 32GB of RAM (Dual-channel, 3200 MT/s) and Windows 11.

CPU frequency turbo boost was disabled (3.2GHz base frequency).
CPU temperature limit was set to 95°C.

|                                | Blake2b [H/s] | Blake2bLong [H/s] | Argon2d [MB/s] | Aes1R [MB/s] | Aes4R [MB/s] | AesHash1R [H/s] | Superscalar [Prog/s] | Dataset [MB/s] | Hash [H/s] | Efficiency [H/Watt/s] |
| ------------------------------ | :-----------: | :---------------: | :------------: | :----------: | :----------: | :-------------: | :------------------: | :------------: | :--------: | :-------------------: |
| RandomX-1.2.1 (102f8acf)       |        3.231M |           103.46K |          881.4 |      47402.5 |      11473.4 |           23702 |                 2754 |         ~838.7 |   **4554** |            **~84.33** |
| modernRX 0.7.1                 |    **5.449M** |       **172.14K** |     **1234.4** |  **47465.2** |  **11882.1** |       **23725** |             **9698** |     **1247.6** |       3043 |                ~62.74 |

Original RandomX provides benchmark only for calculating final hashes. All other values were estimated (based on information benchmark provides) or some custom benchmarks were written on top of RandomX implementation, thus values may not be 100% accurate.

For details and full benchmark results see [BENCHMARKS.md](BENCHMARKS.md).

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

* **v0.7.1 - 28.11.2023:** optimize hash calculation (thread affinity, scratchpad hash and fill)
* ...
* **v0.1.3 - 29.10.2023:** bugfixes, benchmarks corrections, code cleanup
* ...
* **v0.1.0 - 03.09.2023:** reference implementation
* **v0.0.1 - 10.08.2023:** initial implementation

For more details see [CHANGELOG.md](CHANGELOG.md).

### Code statistics (via [gocloc](https://github.com/hhatto/gocloc))

```console
$> gocloc /exclude-ext "xml,json,txt,exp" /not-match-d "3rdparty/*|x64/*|assets/*" .
-------------------------------------------------------------------------------
Language                     files          blank        comment           code
-------------------------------------------------------------------------------
C++                             17            607            381           3459
C++ Header                      35            527            577           3043
Markdown                         3            178              0            527
-------------------------------------------------------------------------------
TOTAL                           55           1312            958           7029
-------------------------------------------------------------------------------
```

## Support

If you like this project and would like to support further development you could donate some cryptos to one of those addresses:

* **BTC:** `bc1qrlw6kdtnwka2ww6j9cvzugs7sk8zk2ewp3mrd6`
* **ETH/USDT:** `0x3679B13D96DF9291eB98d1308008Be73F8D05b5B`
* **XMR:** `43hGi1uJDmbGE7RUZJoBLa6BnCDtT3tNdb7v7VMQbheohso81CoUMeJTA7wcHt9Xi27Cw8tPGPex55mpoT3q46MGMa7ca6m`

All donations are voluntary and i will be grateful for every single satoshi.

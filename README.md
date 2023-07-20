# modernRX - Modern C++ RandomX Implementation

## Overview

This project aims to provide minimal implementation of [RandomX algorithm](https://github.com/tevador/RandomX) using modern C++ language.

This repository is not, and never will be, focused to implement a standalone miner, but a library for RandomX hash calculation.

Current state of this project does not provide sensible performance to use in mining, but you can still use this repository for educational purposes (or whatever that will make sense for you).

## Features

* Self-contained: no external dependencies and most of the code is written from scratch based on RandomX specs, design and source code.
* Modern: written with most recent C++ features in mind (C\++20 and C\++23).
* Understandable: code is much more concise, readable and better documented than original RandomX implementation.
* Minimal: as little code as possible to be able to run on most recent Intel/AMD CPUs (in exchange for portability).

## Roadmap

* [x] (10.08.2023) Provide minimal implementation that yield correct RandomX hashes.
* [ ] Polish tests and documentation, add benchmarks.
* [ ] Optimize dataset generation with multithreading and hardware specific instructions.
* [ ] Optimize dataset generation with JIT compiler for superscalar programs.
* [ ] Optimize hash calculation with JIT compiler for random programs.
* [ ] Optimize hash calculation with multithreading.
* [ ] Experiment with further JIT optimizations for faster hash calculation.
* [ ] Experiment with system and architecture specific optimizations (Huge Pages, MSR etc.) for faster hash calculation.
* [ ] Implement RandomX light mode.
* [ ] Implement RandomX GPU mode.
* [ ] Port library to Unix-based systems.

## Build and run

To build this repository you should download the most recent Visual Studio version (at least 17.7) with C++ tools.

To run tests open solution, set `tests` project as the startup one and click "run". Currently tests will run for about an hour, because of unoptimized dataset generation.

### Portability

No plans for support multi OSes, platforms or architectures in the nearest future and the only guaranteed environment is the one i develop on, which is:

* System: Windows 11
* CPU: Zen 3 (Ryzen 5800H)

But it should work with Windows 7 and higher and any 64-bit little-endian CPU.

## Quick start

This repository is meant to be build as a library that can be linked into other programs, specifically Monero miner programs.

No code examples available at this moment, for details check `tests.cpp` file.

## Coding guidelines

Code found in this repository follows guidelines listed below:

* Use most recent C++ features wherever possible (mostly for educational purposes).
* Do not use external dependencies (only STL and compiler intrinsics allowed).
* Do not allow compiler warnings in master branch (at least /W3 level).
* Avoid using preprocessor and macros. Every exception must be documented.
* Describe all headers at the beginning of file, right before imports.
* Document all functions and types in headers. If function is declared in .cpp prefer documentation close to the definition, not declaration.
* Every optimization decision must be thoroughly explained and documented (mostly for educational purposes).
* Preallocate data buffers wherever possible.
* Prefer reusing memory with output parameters instead of returned ones wherever possible.
* If output parameter is chosen over returned ones, it must be the first argument in function.
* Use consteval/constexpr/const wherever possible.
* Constexpr variables used only within single functions should be defined in that function.
* Use inline for constexpr variables in headers. All exceptions from this rule must be documented.
* Prefer using unnamed namespaces over static functions and private methods that does not require access to class field.
* All functions that belongs to unnamed namespaces must be declared at the beginning and defined at the end of a .cpp file.
* Prefer spans and ranges instead of arrays and vectors as function parameters.
* Use [[no_discard]], noexcept and explicit wherever possible.
* Do not use [[unlikely]] and [[likely]].
* Functions with single boolean parameter should be splitted into two separate functions or implemented as a single template function with boolean template parameter.
* It is allowed for inner functions to be stored as lambdas within outer functions that defines that lambda, if there is no other valid usage of such lambda.

## Versioning

Project follows [zero-based versioning](https://0ver.org/) with several specific versions:

* `0.0.1` - initial implementation
* `0.1.0` - reference implementation (contains polished documentation and tests + benchmarks; reference point to further tweaks and optimizations)
* `0.1.x` - reference implementation++ (contains bugfixes, documentation fixes, coding standard updates etc.)

## Changelog

* **v0.0.1 - 10.08.2023:** initial implementation

### Code statistics (via [gocloc](https://github.com/hhatto/gocloc))

```console
$> gocloc /exclude-ext xml,json,txt .
-------------------------------------------------------------------------------
Language                     files          blank        comment           code
-------------------------------------------------------------------------------
C++                             13            419            201           2044
C++ Header                      20            194            175            995
Markdown                         1             32              0             81
-------------------------------------------------------------------------------
TOTAL                           34            645            376           3120
-------------------------------------------------------------------------------
```

## Support

If you like this project and would like to support further development you could donate some cryptos to one of those addresses:

* **BTC:** `bc1qrlw6kdtnwka2ww6j9cvzugs7sk8zk2ewp3mrd6`
* **ETH/USDT:** `0x3679B13D96DF9291eB98d1308008Be73F8D05b5B`
* **XMR:** `43hGi1uJDmbGE7RUZJoBLa6BnCDtT3tNdb7v7VMQbheohso81CoUMeJTA7wcHt9Xi27Cw8tPGPex55mpoT3q46MGMa7ca6m`

Any donations are voluntary, but highly appreciated.

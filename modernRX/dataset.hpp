#pragma once


/*
* Multi-threaded, JIT-compiled and AVX2 enhanced implementation of RandomX Dataset: https://github.com/tevador/RandomX/blob/master/doc/specs.md#7-dataset
* This is used as read-only memory by RandomX programs to calculate hashes.
*/

#include "argon2d.hpp"
#include "heaparray.hpp"
#include "jitcompiler.hpp"
#include "randomxparams.hpp"
#include "superscalar.hpp"

namespace modernRX {
    // Compiles superscalar programs and fills read-only memory used by RandomX programs to calculate hashes according to https://github.com/tevador/RandomX/blob/master/doc/specs.md#7-dataset.
    // Needs cache as an Argon2d filled memory buffer and 8 superscalar programs.
    // May throw.
    [[nodiscard]] HeapArray<DatasetItem, 4096> generateDataset(const_span<argon2d::Block> cache, const_span<SuperscalarProgram, Rx_Cache_Accesses> programs);
}

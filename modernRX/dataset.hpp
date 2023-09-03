#pragma once


/*
* Single-threaded and no SIMD supported implementation of RandomX Dataset: https://github.com/tevador/RandomX/blob/master/doc/specs.md#7-dataset
* This is used as read-only memory by RandomX programs to calculate hashes.
*/

#include "aliases.hpp"
#include "argon2d.hpp"
#include "randomxparams.hpp"
#include "superscalar.hpp"

namespace modernRX {
    // DatasetItem defines single cache and dataset item used during generation. Must be 64-bytes size.
    using DatasetItem = std::array<uint64_t, 8>;
    static_assert(sizeof(DatasetItem) == 64);

    // Fills read-only memory used by RandomX programs to calculate hashes according to https://github.com/tevador/RandomX/blob/master/doc/specs.md#7-dataset.
    // Needs cache as an argon2d filled memory buffer and 8 superscalar programs.
    [[nodiscard]] std::vector<DatasetItem> generateDataset(const_span<argon2d::Block> cache, const_span<Program, Rx_Cache_Accesses> programs);
}
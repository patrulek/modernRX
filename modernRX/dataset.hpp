#pragma once

#include <span>
#include <vector>

#include "argon2d.hpp"
#include "superscalar.hpp"


namespace modernRX {
    // DatasetItem defines single cache and dataset item used during generation. Must be 64-bytes size.
    using DatasetItem = std::array<uint64_t, 8>;
    static_assert(sizeof(DatasetItem) == 64);

    // DatasetMemory defines dataset that consists of DatasetItem.
    using DatasetMemory = std::vector<DatasetItem>;

    // Number of superscalar programs used to generate dataset.
    inline constexpr uint32_t Program_Count{ 8 };

    // Fills read-only memory used by RandomX programs to calculate hashes according to https://github.com/tevador/RandomX/blob/master/doc/specs.md#7-dataset.
    // Needs cache_memory as an argon2d filled memory buffer and 8 superscalar programs.
    DatasetMemory generateDataset(const argon2d::Memory& cache_memory, const std::array<Program, Program_Count>& programs);
}
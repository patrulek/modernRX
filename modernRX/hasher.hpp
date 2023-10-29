#pragma once

/*
* Single-threaded RandomX hash generator.
*/

#include "dataset.hpp"
#include "interpreter.hpp"

namespace modernRX {
    class Hasher {
    public:
        // Initialize with empty key (for later reset).
        [[nodiscard]] explicit Hasher() = default;
        
        // Initialize with key to generate Dataset at creation
        [[nodiscard]] explicit Hasher(const_span<std::byte> key);

        // Generate hash from input data.
        [[nodiscard]] std::array<std::byte, 32> run(const_span<std::byte> input);

        // Resets Dataset with new key. Does nothing if key is equal to previous one.
        void reset(const_span<std::byte> key);
    private:
        std::vector<std::byte> key; // Latest key used for Dataset generation.
        std::vector<DatasetItem> dataset; // Dataset used for program execution.
    };
}

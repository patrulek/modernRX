#pragma once

/*
* Definition of RandomX scratchpad: https://github.com/tevador/RandomX/blob/master/doc/specs.md#42-scratchpad
* This is used by RandomX virtual machine during program execution as a cache memory.
*/

#include <span>

#include "heaparray.hpp"

namespace modernRX {
    // Defines interpreter's cache memory for reading and writing operations.
    class Scratchpad {
    public:
        // Initializes scratchpad with a seed.
        // After initialization seed is replaced with last 64 bytes of scratchpad.
        [[nodiscard]] explicit Scratchpad(std::span<std::byte, 64> seed);

        // Reads a single value from scratchpad at given offset.
        // T has to be uint64_t or intrinsics::xmm128d_t.
        template<typename T>
        [[nodiscard]] T read(const uint64_t offset) const noexcept;

        // Writes a value to scratchpad at given offset.
        void write(const uint64_t offset, const void* value, const size_t size) noexcept;

        // Returns address of scratchpad's memory.
        [[nodiscard]] const std::byte* data() const noexcept;
    private:
        HeapArray<std::byte, 4096> memory;
    };
}

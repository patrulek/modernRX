#pragma once

/*
* Definition of RandomX scratchpad: https://github.com/tevador/RandomX/blob/master/doc/specs.md#42-scratchpad
* This is used by RandomX virtual machine during program execution as a cache memory.
*/

#include <span>
#include <vector>

namespace modernRX {
    // Defines interpreter's cache memory for reading and writing operations.
    class Scratchpad {
    public:
        // Initializes scratchpad with a seed.
        // After initialization seed is replaced with last 64 bytes of scratchpad.
        [[nodiscard]] explicit Scratchpad(std::span<std::byte, 64> seed);

        // Reads a single 8-byte value from scratchpad at given offset.
        [[nodiscard]] uint64_t read(const uint64_t offset) const noexcept;

        // Writes a single 8-byte value to scratchpad at given offset.
        void write(const uint64_t offset, const uint64_t value) noexcept;

        // Returns address of scratchpad's memory.
        [[nodiscard]] const std::byte* data() const noexcept;
    private:
        std::vector<std::byte> memory;
    };
}

#pragma once

#include <vector>
#include <span>

namespace modernRX {
    inline constexpr uint32_t Scratchpad_Size{ 2 * 1024 * 1024 };
    inline constexpr uint32_t Scratchpad_Size_L1{ 16 * 1024 };
    inline constexpr uint32_t Scratchpad_Size_L2{ 256 * 1024 };
    inline constexpr uint32_t ScratchpadL1{ Scratchpad_Size_L1 / 8 };
    inline constexpr uint32_t ScratchpadL2{ Scratchpad_Size_L2 / 8 };
    inline constexpr uint32_t ScratchpadL3{ Scratchpad_Size / 8 };
    inline constexpr uint32_t ScratchpadL1Mask{ (ScratchpadL1 - 1) * 8 };
    inline constexpr uint32_t ScratchpadL2Mask{ (ScratchpadL2 - 1) * 8 };
    inline constexpr uint32_t ScratchpadL1Mask16{ (ScratchpadL1 / 2 - 1) * 16 };
    inline constexpr uint32_t ScratchpadL2Mask16{ (ScratchpadL2 / 2 - 1) * 16 };
    inline constexpr uint32_t ScratchpadL3Mask{ (ScratchpadL3 - 1) * 8 };
    inline constexpr uint32_t ScratchpadL3Mask64{ (ScratchpadL3 / 8 - 1) * 64 };

    class Scratchpad {
    public:
        explicit Scratchpad(std::span<std::byte, 64> seed);

        uint64_t read(const uint64_t offset) const;
        void write(const uint64_t offset, const uint64_t value);
        const std::byte* data() const;
    private:
        std::vector<std::byte> memory;
    };
}
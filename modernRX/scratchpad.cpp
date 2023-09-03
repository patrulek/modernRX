#include "aes1rrandom.hpp"
#include "randomxparams.hpp"
#include "scratchpad.hpp"

namespace modernRX {
    Scratchpad::Scratchpad(std::span<std::byte, 64> seed) {
        memory.resize(Rx_Scratchpad_L3_Size);
        aes::fill1R(memory, seed);

        // Last 64 bytes of the scratchpad are now the new seed.
        std::copy(memory.end() - seed.size(), memory.end(), seed.begin());
    }

    uint64_t Scratchpad::read(const uint64_t offset) const noexcept {
        uint64_t value{ 0 };
        std::memcpy(&value, memory.data() + offset, sizeof(uint64_t));

        return value;
    }

    void Scratchpad::write(const uint64_t offset, const uint64_t value) noexcept {
        std::memcpy(memory.data() + offset, &value, sizeof(uint64_t));
    }


    const std::byte* Scratchpad::data() const noexcept {
        return memory.data();
    }
}
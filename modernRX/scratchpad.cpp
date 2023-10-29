#include "aes1rrandom.hpp"
#include "randomxparams.hpp"
#include "scratchpad.hpp"

namespace modernRX {
    Scratchpad::Scratchpad(std::span<std::byte, 64> seed) {
        memory.resize(Rx_Scratchpad_L3_Size);
        aes::fill1R(memory, seed);

        // Last 64 bytes of the scratchpad memory are now the new seed.
    }

    uint64_t Scratchpad::read(const uint64_t offset) const noexcept {
        return *reinterpret_cast<const uint64_t*>(memory.data() + offset);
    }

    void Scratchpad::write(const uint64_t offset, const uint64_t value) noexcept {
        std::memcpy(memory.data() + offset, &value, sizeof(uint64_t));
    }

    const std::byte* Scratchpad::data() const noexcept {
        return memory.data();
    }
}

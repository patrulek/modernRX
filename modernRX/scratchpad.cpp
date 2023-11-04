#include "aes1rrandom.hpp"
#include "randomxparams.hpp"
#include "scratchpad.hpp"
#include "sse.hpp"

namespace modernRX {
    Scratchpad::Scratchpad(std::span<std::byte, 64> seed) :
        memory(Rx_Scratchpad_L3_Size) {
        aes::fill1R(memory.buffer(), seed);

        // Last 64 bytes of the scratchpad memory are now the new seed.
    }

    template uint64_t Scratchpad::read<uint64_t>(const uint64_t offset) const noexcept;
    template intrinsics::xmm128d_t Scratchpad::read<intrinsics::xmm128d_t>(const uint64_t offset) const noexcept;

    template<typename T>
    T Scratchpad::read(const uint64_t offset) const noexcept {
        if constexpr (std::is_same_v<T, uint64_t>) {
            return *reinterpret_cast<const uint64_t*>(&memory[offset]);
        } else if constexpr (std::is_same_v<T, intrinsics::xmm128d_t>) {
            return intrinsics::sse::vcvtpi32<double>(&memory[offset]);
        } else {
            static_assert(!sizeof(T), "Ret must be either uint64_t or intrinsics::xmm128d_t");
        }
    }

    void Scratchpad::write(const uint64_t offset, const void* value, const size_t size) noexcept {
        std::memcpy(&memory[offset], value, size);
    }

    const std::byte* Scratchpad::data() const noexcept {
        return memory.view().data();
    }
}

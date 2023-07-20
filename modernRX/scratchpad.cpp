#include "scratchpad.hpp"
#include "aes1rrandom.hpp"



namespace modernRX {
    Scratchpad::Scratchpad(std::span<std::byte, 64> seed) {
        memory.resize(Scratchpad_Size);

        aes::Random1R aes1r_random{ seed };
        aes1r_random.fill(memory);

        std::memcpy(seed.data(), aes1r_random.data(), seed.size());
    }


    uint64_t Scratchpad::read(const uint64_t offset) const {
        uint64_t value{ 0 };
        std::memcpy(&value, memory.data() + offset, sizeof(uint64_t));

        return value;
    }

    void Scratchpad::write(const uint64_t offset, const uint64_t value) {
        std::memcpy(memory.data() + offset, &value, sizeof(uint64_t));
    }


    const std::byte* Scratchpad::data() const {
        return memory.data();
    }
}
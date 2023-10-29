#include <format>

#include "aes.hpp"
#include "aes1rrandom.hpp"
#include "assertume.hpp"
#include "cast.hpp"

namespace modernRX::aes {
    namespace {
        // key0, key1, key2, key3 = Blake2b-512("RandomX AesGenerator1R keys")
        // key0 = 53 a5 ac 6d 09 66 71 62 2b 55 b5 db 17 49 f4 b4
        // key1 = 07 af 7c 6d 0d 71 6a 84 78 d3 25 17 4e dc a1 0d
        // key2 = f1 62 12 3f c6 7e 94 9f 4f 79 c0 f4 45 e3 20 3e
        // key3 = 35 81 ef 6a 7c 31 ba b1 88 4c 31 16 54 91 16 49
        //
        // Every key is treated as four 4-byte integers (in little-endian byte order).
        constexpr alignas(64) std::array<std::array<uint32_t, 4>, 4> keys{
            std::array<uint32_t, 4>{ 0x6daca553, 0x62716609, 0xdbb5552b, 0xb4f44917 },
            std::array<uint32_t, 4>{ 0x6d7caf07, 0x846a710d, 0x1725d378, 0x0da1dc4e },
            std::array<uint32_t, 4>{ 0x3f1262f1, 0x9f947ec6, 0xf4c0794f, 0x3e20e345 },
            std::array<uint32_t, 4>{ 0x6aef8135, 0xb1ba317c, 0x16314c88, 0x49169154 },
        };
    }

    void fill1R(std::span<std::byte> output, std::span<std::byte, 64> seed) noexcept {
        ASSERTUME(output.size() % 64 == 0);

        auto state{ span_cast<std::array<uint32_t, 4>, 4>(seed.data()) };

        for (size_t i = 0; i < output.size(); i += 64) {
            decode(state[0], keys[0]);
            encode(state[1], keys[1]);
            decode(state[2], keys[2]);
            encode(state[3], keys[3]);

            std::memcpy(output.data() + i, state.data(), 64);
        }
    }
}

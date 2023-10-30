#include <format>

#include "aes.hpp"
#include "aes1rrandom.hpp"
#include "assertume.hpp"
#include "cast.hpp"
#include "randomxparams.hpp"
#include "sse.hpp"

namespace modernRX::aes {
    template void fill1R<true>(std::span<std::byte> output, std::span<std::byte, 64> seed) noexcept;
    template void fill1R<false>(std::span<std::byte> output, std::span<std::byte, 64> seed) noexcept;

    template<bool Fixed>
    void fill1R(std::span<std::byte> output, std::span<std::byte, 64> seed) noexcept {
        ASSERTUME(output.size() > 0 && output.size() % 64 == 0);

        intrinsics::xmm128i_t& seed0{ *reinterpret_cast<intrinsics::xmm128i_t*>(seed.data()) };
        intrinsics::xmm128i_t& seed1{ *reinterpret_cast<intrinsics::xmm128i_t*>(seed.data() + 16) };
        intrinsics::xmm128i_t& seed2{ *reinterpret_cast<intrinsics::xmm128i_t*>(seed.data() + 32) };
        intrinsics::xmm128i_t& seed3{ *reinterpret_cast<intrinsics::xmm128i_t*>(seed.data() + 48) };

        // key0, key1, key2, key3 = Blake2b-512("RandomX AesGenerator1R keys")
        // key0 = 53 a5 ac 6d 09 66 71 62 2b 55 b5 db 17 49 f4 b4
        // key1 = 07 af 7c 6d 0d 71 6a 84 78 d3 25 17 4e dc a1 0d
        // key2 = f1 62 12 3f c6 7e 94 9f 4f 79 c0 f4 45 e3 20 3e
        // key3 = 35 81 ef 6a 7c 31 ba b1 88 4c 31 16 54 91 16 49
        constexpr auto key0{ intrinsics::fromChars(0x53, 0xa5, 0xac, 0x6d, 0x09, 0x66, 0x71, 0x62, 0x2b, 0x55, 0xb5, 0xdb, 0x17, 0x49, 0xf4, 0xb4) };
        auto state0{ intrinsics::sse::vload<int>(seed.data()) };

        constexpr auto key1{ intrinsics::fromChars(0x07, 0xaf, 0x7c, 0x6d, 0x0d, 0x71, 0x6a, 0x84, 0x78, 0xd3, 0x25, 0x17, 0x4e, 0xdc, 0xa1, 0x0d) };
        auto state1{ intrinsics::sse::vload<int>(seed.data() + 16) };

        constexpr auto key2{ intrinsics::fromChars(0xf1, 0x62, 0x12, 0x3f, 0xc6, 0x7e, 0x94, 0x9f, 0x4f, 0x79, 0xc0, 0xf4, 0x45, 0xe3, 0x20, 0x3e) };
        auto state2{ intrinsics::sse::vload<int>(seed.data() + 32) };

        constexpr auto key3{ intrinsics::fromChars(0x35, 0x81, 0xef, 0x6a, 0x7c, 0x31, 0xba, 0xb1, 0x88, 0x4c, 0x31, 0x16, 0x54, 0x91, 0x16, 0x49) };
        auto state3{ intrinsics::sse::vload<int>(seed.data() + 48) };

        // Switch between fixed and variable output size. 
        for (size_t i = 0; i < (Fixed ? Rx_Scratchpad_L3_Size : output.size()); i += 64) {
            intrinsics::aes::decode(state0, key0);
            intrinsics::aes::encode(state1, key1);
            intrinsics::aes::decode(state2, key2);
            intrinsics::aes::encode(state3, key3);

            intrinsics::xmm128i_t& output0{ *reinterpret_cast<intrinsics::xmm128i_t*>(output.data() + i) };
            intrinsics::xmm128i_t& output1{ *reinterpret_cast<intrinsics::xmm128i_t*>(output.data() + i + 16) };
            intrinsics::xmm128i_t& output2{ *reinterpret_cast<intrinsics::xmm128i_t*>(output.data() + i + 32) };
            intrinsics::xmm128i_t& output3{ *reinterpret_cast<intrinsics::xmm128i_t*>(output.data() + i + 48) };

            output0 = state0;
            output1 = state1;
            output2 = state2;
            output3 = state3;
        }

        seed0 = state0;
        seed1 = state1;
        seed2 = state2;
        seed3 = state3;
    }
}

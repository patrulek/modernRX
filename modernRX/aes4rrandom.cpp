#include <format>

#include "aes.hpp"
#include "aes4rrandom.hpp"
#include "assertume.hpp"
#include "cast.hpp"
#include "randomxparams.hpp"
#include "sse.hpp"

namespace modernRX::aes {
    template void fill4R<true>(std::span<std::byte> output, std::span<std::byte, 64> seed) noexcept;
    template void fill4R<false>(std::span<std::byte> output, std::span<std::byte, 64> seed) noexcept;

    template<bool Fixed>
    void fill4R(std::span<std::byte> output, std::span<std::byte, 64> seed) noexcept {
        ASSERTUME(output.size() > 0 && output.size() % 64 == 0);

        intrinsics::xmm128i_t& seed0{ *reinterpret_cast<intrinsics::xmm128i_t*>(seed.data()) };
        intrinsics::xmm128i_t& seed1{ *reinterpret_cast<intrinsics::xmm128i_t*>(seed.data() + 16) };
        intrinsics::xmm128i_t& seed2{ *reinterpret_cast<intrinsics::xmm128i_t*>(seed.data() + 32) };
        intrinsics::xmm128i_t& seed3{ *reinterpret_cast<intrinsics::xmm128i_t*>(seed.data() + 48) };

        // key0, key1, key2, key3 = Blake2b-512("RandomX AesGenerator4R keys 0-3")
        // key4, key5, key6, key7 = Blake2b-512("RandomX AesGenerator4R keys 4-7")
        // key0 = dd aa 21 64 db 3d 83 d1 2b 6d 54 2f 3f d2 e5 99
        // key1 = 50 34 0e b2 55 3f 91 b6 53 9d f7 06 e5 cd df a5
        // key2 = 04 d9 3e 5c af 7b 5e 51 9f 67 a4 0a bf 02 1c 17
        // key3 = 63 37 62 85 08 5d 8f e7 85 37 67 cd 91 d2 de d8
        // key4 = 73 6f 82 b5 a6 a7 d6 e3 6d 8b 51 3d b4 ff 9e 22
        // key5 = f3 6b 56 c7 d9 b3 10 9c 4e 4d 02 e9 d2 b7 72 b2
        // key6 = e7 c9 73 f2 8b a3 65 f7 0a 66 a9 2b a7 ef 3b f6
        // key7 = 09 d6 7c 7a de 39 58 91 fd d1 06 0c 2d 76 b0 c0
        constexpr auto key0{ intrinsics::fromChars(0xdd, 0xaa, 0x21, 0x64, 0xdb, 0x3d, 0x83, 0xd1, 0x2b, 0x6d, 0x54, 0x2f, 0x3f, 0xd2, 0xe5, 0x99) };
        auto state0{ intrinsics::sse::vload<int>(seed.data()) };
        auto state1{ intrinsics::sse::vload<int>(seed.data() + 16) };

        constexpr auto key4{ intrinsics::fromChars(0x73, 0x6f, 0x82, 0xb5, 0xa6, 0xa7, 0xd6, 0xe3, 0x6d, 0x8b, 0x51, 0x3d, 0xb4, 0xff, 0x9e, 0x22) };
        auto state2{ intrinsics::sse::vload<int>(seed.data() + 32) };
        auto state3{ intrinsics::sse::vload<int>(seed.data() + 48) };

        constexpr auto key1{ intrinsics::fromChars(0x50, 0x34, 0x0e, 0xb2, 0x55, 0x3f, 0x91, 0xb6, 0x53, 0x9d, 0xf7, 0x06, 0xe5, 0xcd, 0xdf, 0xa5) };
        constexpr auto key5{ intrinsics::fromChars(0xf3, 0x6b, 0x56, 0xc7, 0xd9, 0xb3, 0x10, 0x9c, 0x4e, 0x4d, 0x02, 0xe9, 0xd2, 0xb7, 0x72, 0xb2) };
        constexpr auto key2{ intrinsics::fromChars(0x04, 0xd9, 0x3e, 0x5c, 0xaf, 0x7b, 0x5e, 0x51, 0x9f, 0x67, 0xa4, 0x0a, 0xbf, 0x02, 0x1c, 0x17) };
        constexpr auto key6{ intrinsics::fromChars(0xe7, 0xc9, 0x73, 0xf2, 0x8b, 0xa3, 0x65, 0xf7, 0x0a, 0x66, 0xa9, 0x2b, 0xa7, 0xef, 0x3b, 0xf6) };
        constexpr auto key3{ intrinsics::fromChars(0x63, 0x37, 0x62, 0x85, 0x08, 0x5d, 0x8f, 0xe7, 0x85, 0x37, 0x67, 0xcd, 0x91, 0xd2, 0xde, 0xd8) };
        constexpr auto key7{ intrinsics::fromChars(0x09, 0xd6, 0x7c, 0x7a, 0xde, 0x39, 0x58, 0x91, 0xfd, 0xd1, 0x06, 0x0c, 0x2d, 0x76, 0xb0, 0xc0) };

        // Switch between fixed and variable output size. 
        for (size_t i = 0; i < (Fixed ? Rx_Program_Bytes_Size : output.size()); i += 64) {
            intrinsics::aes::decode(state0, key0);
            intrinsics::aes::encode(state1, key0);
            intrinsics::aes::decode(state2, key4);
            intrinsics::aes::encode(state3, key4);
            
            intrinsics::aes::decode(state0, key1);
            intrinsics::aes::encode(state1, key1);
            intrinsics::aes::decode(state2, key5);
            intrinsics::aes::encode(state3, key5);
            
            intrinsics::aes::decode(state0, key2);
            intrinsics::aes::encode(state1, key2);
            intrinsics::aes::decode(state2, key6);
            intrinsics::aes::encode(state3, key6);
            
            intrinsics::aes::decode(state0, key3);
            intrinsics::aes::encode(state1, key3);
            intrinsics::aes::decode(state2, key7);
            intrinsics::aes::encode(state3, key7);

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

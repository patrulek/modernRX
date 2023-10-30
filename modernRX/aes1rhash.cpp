#include <array>
#include <bit>
#include <format>

#include "aes.hpp"
#include "aes1rhash.hpp"
#include "assertume.hpp"
#include "cast.hpp"
#include "randomxparams.hpp"

namespace modernRX::aes {
    template void hash1R<true>(std::span<std::byte, 64> output, const_span<std::byte> input) noexcept;
    template void hash1R<false>(std::span<std::byte, 64> output, const_span<std::byte> input) noexcept;

    template<bool Fixed>
    void hash1R(std::span<std::byte, 64> output, const_span<std::byte> input) noexcept {
        ASSERTUME(input.size() > 0 && input.size() % 64 == 0);

        // state0, state1, state2, state3 = Blake2b-512("RandomX AesHash1R state")
        // state0 = 0d 2c b5 92 de 56 a8 9f 47 db 82 cc ad 3a 98 d7
        // state1 = 6e 99 8d 33 98 b7 c7 15 5a 12 9e f5 57 80 e7 ac
        // state2 = 17 00 77 6a d0 c7 62 ae 6b 50 79 50 e4 7c a0 e8
        // state3 = 0c 24 0a 63 8d 82 ad 07 05 00 a1 79 48 49 99 7e
        auto state0{ intrinsics::fromChars(0x0d, 0x2c, 0xb5, 0x92, 0xde, 0x56, 0xa8, 0x9f, 0x47, 0xdb, 0x82, 0xcc, 0xad, 0x3a, 0x98, 0xd7) };
        auto state1{ intrinsics::fromChars(0x6e, 0x99, 0x8d, 0x33, 0x98, 0xb7, 0xc7, 0x15, 0x5a, 0x12, 0x9e, 0xf5, 0x57, 0x80, 0xe7, 0xac) };
        auto state2{ intrinsics::fromChars(0x17, 0x00, 0x77, 0x6a, 0xd0, 0xc7, 0x62, 0xae, 0x6b, 0x50, 0x79, 0x50, 0xe4, 0x7c, 0xa0, 0xe8) };
        auto state3{ intrinsics::fromChars(0x0c, 0x24, 0x0a, 0x63, 0x8d, 0x82, 0xad, 0x07, 0x05, 0x00, 0xa1, 0x79, 0x48, 0x49, 0x99, 0x7e) };

        // Switch between fixed and variable output size. 
        for (uint64_t i = 0; i < (Fixed ? Rx_Scratchpad_L3_Size : input.size()); i += 64) {
            const intrinsics::xmm128i_t& input0{ *reinterpret_cast<const intrinsics::xmm128i_t*>(input.data() + i) };
            const intrinsics::xmm128i_t& input1{ *reinterpret_cast<const intrinsics::xmm128i_t*>(input.data() + i + 16) };
            const intrinsics::xmm128i_t& input2{ *reinterpret_cast<const intrinsics::xmm128i_t*>(input.data() + i + 32) };
            const intrinsics::xmm128i_t& input3{ *reinterpret_cast<const intrinsics::xmm128i_t*>(input.data() + i + 48) };
        
            intrinsics::aes::encode(state0, input0);
            intrinsics::aes::decode(state1, input1);
            intrinsics::aes::encode(state2, input2);
            intrinsics::aes::decode(state3, input3);
        }

        // xkey0, xkey1 = Blake2b-256("RandomX AesHash1R xkeys")
        // xkey0 = 89 83 fa f6 9f 94 24 8b bf 56 dc 90 01 02 89 06
        // xkey1 = d1 63 b2 61 3c e0 f4 51 c6 43 10 ee 9b f9 18 ed
        constexpr auto key0{ intrinsics::fromChars(0x89, 0x83, 0xfa, 0xf6, 0x9f, 0x94, 0x24, 0x8b, 0xbf, 0x56, 0xdc, 0x90, 0x01, 0x02, 0x89, 0x06) };
        constexpr auto key1{ intrinsics::fromChars(0xd1, 0x63, 0xb2, 0x61, 0x3c, 0xe0, 0xf4, 0x51, 0xc6, 0x43, 0x10, 0xee, 0x9b, 0xf9, 0x18, 0xed) };
        
        intrinsics::aes::encode(state0, key0);
        intrinsics::aes::decode(state1, key0);
        intrinsics::aes::encode(state2, key0);
        intrinsics::aes::decode(state3, key0);
        
        intrinsics::aes::encode(state0, key1);
        intrinsics::aes::decode(state1, key1);
        intrinsics::aes::encode(state2, key1);
        intrinsics::aes::decode(state3, key1);
        
        intrinsics::xmm128i_t& output0{ *reinterpret_cast<intrinsics::xmm128i_t*>(output.data()) };
        intrinsics::xmm128i_t& output1{ *reinterpret_cast<intrinsics::xmm128i_t*>(output.data() + 16) };
        intrinsics::xmm128i_t& output2{ *reinterpret_cast<intrinsics::xmm128i_t*>(output.data() + 32) };
        intrinsics::xmm128i_t& output3{ *reinterpret_cast<intrinsics::xmm128i_t*>(output.data() + 48) };

        output0 = state0;
        output1 = state1;
        output2 = state2;
        output3 = state3;
    }
}

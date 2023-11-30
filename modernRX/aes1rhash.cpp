#include <array>
#include <bit>
#include <format>

#include "aes.hpp"
#include "aes1rhash.hpp"
#include "assertume.hpp"
#include "cast.hpp"
#include "randomxparams.hpp"
#include "sse.hpp"

namespace modernRX::aes {
    template void hash1R<true>(std::span<std::byte, 64> output, const_span<std::byte> input) noexcept;
    template void hash1R<false>(std::span<std::byte, 64> output, const_span<std::byte> input) noexcept;


    void hashAndFill1R(std::span<std::byte, 64> hash, std::span<std::byte, 64> seed, std::span<std::byte> scratchpad) noexcept {

        // state0, state1, state2, state3 = Blake2b-512("RandomX AesHash1R state")
        // state0 = 0d 2c b5 92 de 56 a8 9f 47 db 82 cc ad 3a 98 d7
        // state1 = 6e 99 8d 33 98 b7 c7 15 5a 12 9e f5 57 80 e7 ac
        // state2 = 17 00 77 6a d0 c7 62 ae 6b 50 79 50 e4 7c a0 e8
        // state3 = 0c 24 0a 63 8d 82 ad 07 05 00 a1 79 48 49 99 7e
        auto alignas(16) hash_state0{ intrinsics::fromChars(0x0d, 0x2c, 0xb5, 0x92, 0xde, 0x56, 0xa8, 0x9f, 0x47, 0xdb, 0x82, 0xcc, 0xad, 0x3a, 0x98, 0xd7) };
        auto alignas(16) hash_state1{ intrinsics::fromChars(0x6e, 0x99, 0x8d, 0x33, 0x98, 0xb7, 0xc7, 0x15, 0x5a, 0x12, 0x9e, 0xf5, 0x57, 0x80, 0xe7, 0xac) };
        auto alignas(16) hash_state2{ intrinsics::fromChars(0x17, 0x00, 0x77, 0x6a, 0xd0, 0xc7, 0x62, 0xae, 0x6b, 0x50, 0x79, 0x50, 0xe4, 0x7c, 0xa0, 0xe8) };
        auto alignas(16) hash_state3{ intrinsics::fromChars(0x0c, 0x24, 0x0a, 0x63, 0x8d, 0x82, 0xad, 0x07, 0x05, 0x00, 0xa1, 0x79, 0x48, 0x49, 0x99, 0x7e) };


        intrinsics::xmm128i_t alignas(16) seed0{ *reinterpret_cast<intrinsics::xmm128i_t*>(seed.data()) };
        intrinsics::xmm128i_t alignas(16) seed1{ *reinterpret_cast<intrinsics::xmm128i_t*>(seed.data() + 16) };
        intrinsics::xmm128i_t alignas(16) seed2{ *reinterpret_cast<intrinsics::xmm128i_t*>(seed.data() + 32) };
        intrinsics::xmm128i_t alignas(16) seed3{ *reinterpret_cast<intrinsics::xmm128i_t*>(seed.data() + 48) };

        // key0, key1, key2, key3 = Blake2b-512("RandomX AesGenerator1R keys")
        // key0 = 53 a5 ac 6d 09 66 71 62 2b 55 b5 db 17 49 f4 b4
        // key1 = 07 af 7c 6d 0d 71 6a 84 78 d3 25 17 4e dc a1 0d
        // key2 = f1 62 12 3f c6 7e 94 9f 4f 79 c0 f4 45 e3 20 3e
        // key3 = 35 81 ef 6a 7c 31 ba b1 88 4c 31 16 54 91 16 49
        constexpr auto alignas(16) key0{ intrinsics::fromChars(0x53, 0xa5, 0xac, 0x6d, 0x09, 0x66, 0x71, 0x62, 0x2b, 0x55, 0xb5, 0xdb, 0x17, 0x49, 0xf4, 0xb4) };
        constexpr auto alignas(16) key1{ intrinsics::fromChars(0x07, 0xaf, 0x7c, 0x6d, 0x0d, 0x71, 0x6a, 0x84, 0x78, 0xd3, 0x25, 0x17, 0x4e, 0xdc, 0xa1, 0x0d) };
        constexpr auto alignas(16) key2{ intrinsics::fromChars(0xf1, 0x62, 0x12, 0x3f, 0xc6, 0x7e, 0x94, 0x9f, 0x4f, 0x79, 0xc0, 0xf4, 0x45, 0xe3, 0x20, 0x3e) };
        constexpr auto alignas(16) key3{ intrinsics::fromChars(0x35, 0x81, 0xef, 0x6a, 0x7c, 0x31, 0xba, 0xb1, 0x88, 0x4c, 0x31, 0x16, 0x54, 0x91, 0x16, 0x49) };

        const auto sp_ptr{ reinterpret_cast<uintptr_t>(scratchpad.data()) };

        //process 64 bytes at a time in 4 lanes 
        for (uint64_t i = 0; i < Rx_Scratchpad_L3_Size; i += 64) {
            intrinsics::xmm128i_t& scratchpad0{ *reinterpret_cast<intrinsics::xmm128i_t*>(scratchpad.data() + i) };
            intrinsics::xmm128i_t& scratchpad1{ *reinterpret_cast<intrinsics::xmm128i_t*>(scratchpad.data() + i + 16) };
            intrinsics::xmm128i_t& scratchpad2{ *reinterpret_cast<intrinsics::xmm128i_t*>(scratchpad.data() + i + 32) };
            intrinsics::xmm128i_t& scratchpad3{ *reinterpret_cast<intrinsics::xmm128i_t*>(scratchpad.data() + i + 48) };

            intrinsics::aes::encode(hash_state0, scratchpad0);
            intrinsics::aes::decode(hash_state1, scratchpad1);
            intrinsics::aes::encode(hash_state2, scratchpad2);
            intrinsics::aes::decode(hash_state3, scratchpad3);

            intrinsics::aes::decode(seed0, key0);
            intrinsics::aes::encode(seed1, key1);
            intrinsics::aes::decode(seed2, key2);
            intrinsics::aes::encode(seed3, key3);

            scratchpad0 = seed0;
            scratchpad1 = seed1;
            scratchpad2 = seed2;
            scratchpad3 = seed3;

            constexpr auto Rf_Size{ 256 }; // TODO: this shouldnt be here.
            intrinsics::prefetch<intrinsics::PrefetchMode::T0, 1>(reinterpret_cast<const void*>(sp_ptr - Rf_Size + ((i + Rf_Size + Rx_Scratchpad_L1_Size) % (Rx_Scratchpad_L3_Size + Rf_Size))));
        }


        // xkey0, xkey1 = Blake2b-256("RandomX AesHash1R xkeys")
        // xkey0 = 89 83 fa f6 9f 94 24 8b bf 56 dc 90 01 02 89 06
        // xkey1 = d1 63 b2 61 3c e0 f4 51 c6 43 10 ee 9b f9 18 ed
        constexpr auto alignas(16) xkey0{ intrinsics::fromChars(0x89, 0x83, 0xfa, 0xf6, 0x9f, 0x94, 0x24, 0x8b, 0xbf, 0x56, 0xdc, 0x90, 0x01, 0x02, 0x89, 0x06) };
        constexpr auto alignas(16) xkey1{ intrinsics::fromChars(0xd1, 0x63, 0xb2, 0x61, 0x3c, 0xe0, 0xf4, 0x51, 0xc6, 0x43, 0x10, 0xee, 0x9b, 0xf9, 0x18, 0xed) };

        intrinsics::aes::encode(hash_state0, xkey0);
        intrinsics::aes::decode(hash_state1, xkey0);
        intrinsics::aes::encode(hash_state2, xkey0);
        intrinsics::aes::decode(hash_state3, xkey0);

        intrinsics::aes::encode(hash_state0, xkey1);
        intrinsics::aes::decode(hash_state1, xkey1);
        intrinsics::aes::encode(hash_state2, xkey1);
        intrinsics::aes::decode(hash_state3, xkey1);

        intrinsics::xmm128i_t& output0{ *reinterpret_cast<intrinsics::xmm128i_t*>(hash.data()) };
        intrinsics::xmm128i_t& output1{ *reinterpret_cast<intrinsics::xmm128i_t*>(hash.data() + 16) };
        intrinsics::xmm128i_t& output2{ *reinterpret_cast<intrinsics::xmm128i_t*>(hash.data() + 32) };
        intrinsics::xmm128i_t& output3{ *reinterpret_cast<intrinsics::xmm128i_t*>(hash.data() + 48) };

        output0 = hash_state0;
        output1 = hash_state1;
        output2 = hash_state2;
        output3 = hash_state3;

        intrinsics::xmm128i_t& output_seed0{ *reinterpret_cast<intrinsics::xmm128i_t*>(seed.data()) };
        intrinsics::xmm128i_t& output_seed1{ *reinterpret_cast<intrinsics::xmm128i_t*>(seed.data() + 16) };
        intrinsics::xmm128i_t& output_seed2{ *reinterpret_cast<intrinsics::xmm128i_t*>(seed.data() + 32) };
        intrinsics::xmm128i_t& output_seed3{ *reinterpret_cast<intrinsics::xmm128i_t*>(seed.data() + 48) };

        output_seed0 = seed0;
        output_seed1 = seed1;
        output_seed2 = seed2;
        output_seed3 = seed3;
    }

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

#pragma once

/*
* Blake2b AVX2 single round function implementation based on libsodium implementation: 
* https://github.com/jedisct1/libsodium/blob/1.0.16/src/libsodium/crypto_generichash/blake2b/ref/blake2b-compress-avx2.h
*/

#include "avx2.hpp"

namespace modernRX::blake2b {
    // Exception from rule to not use macros.
    // Inlining this function is crucial for performance and i wasnt able to force compiler to inline it just with template function.
    // Making a call for each round severly slows down algorithm(even 10-20x).
    //
    // vshuffleepi32 performs right rotation by 32 bits.
    // vshuffleepi8 performs right rotation by 24 or 16 bits accordingly to mask.
    // vpermuteepi64 performs rotation by 64, 128 or 192 bits.
#define ROUND(round, msg, v1, v2, v3, v4, rot24, rot16) do {                        \
/*G1V1*/                                                                            \
        v1 = intrinsics::avx2::vadd<uint64_t>(v1, m);                               \
        v1 = intrinsics::avx2::vadd<uint64_t>(v1, v2);                              \
        m = msgPermutation<1, round>(msg);                                          \
        v4 = intrinsics::avx2::vxor<uint64_t>(v4, v1);                              \
        v4 = intrinsics::avx2::vshuffleepi32<uint64_t, 0b10'11'00'01>(v4);          \
        v3 = intrinsics::avx2::vadd<uint64_t>(v3, v4);                              \
        v2 = intrinsics::avx2::vxor<uint64_t>(v2, v3);                              \
        v2 = intrinsics::avx2::vshuffleepi8<uint64_t>(v2, rot24);                   \
/*G2V1*/                                                                            \
        v1 = intrinsics::avx2::vadd<uint64_t>(v1, m);                               \
        v1 = intrinsics::avx2::vadd<uint64_t>(v1, v2);                              \
        m = msgPermutation<2, round>(msg);                                          \
        v4 = intrinsics::avx2::vxor<uint64_t>(v4, v1);                              \
        v4 = intrinsics::avx2::vshuffleepi8<uint64_t>(v4, rot16);                   \
        v3 = intrinsics::avx2::vadd<uint64_t>(v3, v4);                              \
        v2 = intrinsics::avx2::vxor<uint64_t>(v2, v3);                              \
        v2 = intrinsics::avx2::vrorpi64<uint64_t, 63>(v2);                          \
/*DIAG_V1*/                                                                         \
        v2 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b00'11'10'01>(v2);          \
        v4 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'01'00'11>(v4);          \
        v3 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b01'00'11'10>(v3);          \
/*G1V1*/                                                                            \
        v1 = intrinsics::avx2::vadd<uint64_t>(v1, m);                               \
        v1 = intrinsics::avx2::vadd<uint64_t>(v1, v2);                              \
        m = msgPermutation<3, round>(msg);                                          \
        v4 = intrinsics::avx2::vxor<uint64_t>(v1, v4);                              \
        v4 = intrinsics::avx2::vshuffleepi32<uint64_t, 0b10'11'00'01>(v4);          \
        v3 = intrinsics::avx2::vadd<uint64_t>(v3, v4);                              \
        v2 = intrinsics::avx2::vxor<uint64_t>(v2, v3);                              \
        v2 = intrinsics::avx2::vshuffleepi8<uint64_t>(v2, rot24);                   \
/*G2V1*/                                                                            \
        v1 = intrinsics::avx2::vadd<uint64_t>(v1, m);                               \
        v1 = intrinsics::avx2::vadd<uint64_t>(v1, v2);                              \
        m = msgPermutation<0, (round + 1) % 10>(msg);                               \
        v4 = intrinsics::avx2::vxor<uint64_t>(v4, v1);                              \
        v4 = intrinsics::avx2::vshuffleepi8<uint64_t>(v4, rot16);                   \
        v3 = intrinsics::avx2::vadd<uint64_t>(v3, v4);                              \
        v2 = intrinsics::avx2::vxor<uint64_t>(v2, v3);                              \
        v2 = intrinsics::avx2::vrorpi64<uint64_t, 63>(v2);                          \
/*UNDIAG_V1*/                                                                       \
        v2 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'01'00'11>(v2);          \
        v4 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b00'11'10'01>(v4);          \
        v3 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b01'00'11'10>(v3);          \
} while(0);


    // Permute the message words for avx2 enhanced version.
    // Its important that this function was always inlined otherwise it may hurt performance.
    // There's no way to force inlining, so it would be safier to use macro, but it seems that compiler is smart enough to inline it anyway.
    // Additional check for generated binary would also be nice to ensure that this function was in fact inlined.
    template <uint32_t N, uint32_t Round>
    [[nodiscard]] constexpr intrinsics::avx2::ymm<uint64_t> msgPermutation(const_span<intrinsics::avx2::ymm<uint64_t>, 8> m) noexcept {
        intrinsics::avx2::ymm<uint64_t> t0, t1;

        if constexpr (N == 0) {
            if constexpr (Round == 0) {
                t0 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[0], m[1]);
                t1 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[2], m[3]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 1) {
                t0 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[7], m[2]);
                t1 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[4], m[6]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 2) {
                t0 = intrinsics::avx2::valignrepi8<uint64_t, 8>(m[6], m[5]);
                t1 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[2], m[7]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 3) {
                t0 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[3], m[1]);
                t1 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[6], m[5]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 4) {
                t0 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[4], m[2]);
                t1 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[1], m[5]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 5) {
                t0 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[1], m[3]);
                t1 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[0], m[4]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 6) {
                t0 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(m[0], m[6]);
                t1 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[7], m[2]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 7) {
                t0 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[6], m[3]);
                t1 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(m[1], m[6]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 8) {
                t0 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[3], m[7]);
                t1 = intrinsics::avx2::valignrepi8<uint64_t, 8>(m[0], m[5]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 9) {
                t0 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[5], m[4]);
                t1 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[3], m[0]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else {
                static_assert(!Round, "Invalid round number");
            }
        } else if constexpr (N == 1) {
            if constexpr (Round == 0) {
                t0 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[0], m[1]);
                t1 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[2], m[3]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 1) {
                t0 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[5], m[4]);
                t1 = intrinsics::avx2::valignrepi8<uint64_t, 8>(m[3], m[7]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 2) {
                t0 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[4], m[0]);
                t1 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(m[6], m[1]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 3) {
                t0 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[4], m[0]);
                t1 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[6], m[7]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 4) {
                t0 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(m[3], m[0]);
                t1 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(m[7], m[2]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 5) {
                t0 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[6], m[5]);
                t1 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[5], m[1]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 6) {
                t0 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[2], m[7]);
                t1 = intrinsics::avx2::valignrepi8<uint64_t, 8>(m[5], m[6]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 7) {
                t0 = intrinsics::avx2::valignrepi8<uint64_t, 8>(m[7], m[5]);
                t1 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[0], m[4]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 8) {
                t0 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[7], m[4]);
                t1 = intrinsics::avx2::valignrepi8<uint64_t, 8>(m[4], m[1]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 9) {
                t0 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[1], m[2]);
                t1 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(m[2], m[3]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else {
                static_assert(!Round, "Invalid round number");
            }
        } else if constexpr (N == 2) {
            if constexpr (Round == 0) {
                t0 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[4], m[5]);
                t1 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[6], m[7]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 1) {
                t0 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'11'00'01>(m[0]);
                t1 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[5], m[2]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 2) {
                t0 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(m[1], m[5]);
                t1 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[3], m[4]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 3) {
                t0 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(m[2], m[1]);
                t1 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(m[7], m[2]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 4) {
                t0 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(m[5], m[7]);
                t1 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(m[1], m[3]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 5) {
                t0 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(m[3], m[2]);
                t1 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[7], m[0]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 6) {
                t0 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[0], m[3]);
                t1 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'11'00'01>(m[4]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 7) {
                t0 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[2], m[7]);
                t1 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[4], m[1]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 8) {
                t0 = m[6];
                t1 = intrinsics::avx2::valignrepi8<uint64_t, 8>(m[5], m[0]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 9) {
                t0 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[7], m[4]);
                t1 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[1], m[6]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else {
                static_assert(!Round, "Invalid round number");
            }
        } else if constexpr (N == 3) {
            if constexpr (Round == 0) {
                t0 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[4], m[5]);
                t1 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[6], m[7]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 1) {
                t0 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[6], m[1]);
                t1 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[3], m[1]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 2) {
                t0 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[7], m[3]);
                t1 = intrinsics::avx2::valignrepi8<uint64_t, 8>(m[2], m[0]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 3) {
                t0 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[3], m[5]);
                t1 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[0], m[4]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 4) {
                t0 = intrinsics::avx2::valignrepi8<uint64_t, 8>(m[6], m[0]);
                t1 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(m[6], m[4]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 5) {
                t0 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[6], m[2]);
                t1 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(m[4], m[7]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 6) {
                t0 = intrinsics::avx2::vunpackhiepi64<uint64_t>(m[3], m[1]);
                t1 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(m[5], m[1]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 7) {
                t0 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[0], m[2]);
                t1 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[3], m[5]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 8) {
                t0 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(m[3], m[1]);
                t1 = m[2];
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else if constexpr (Round == 9) {
                t0 = intrinsics::avx2::valignrepi8<uint64_t, 8>(m[7], m[5]);
                t1 = intrinsics::avx2::vunpackloepi64<uint64_t>(m[6], m[0]);
                return intrinsics::avx2::vblendepi32<uint64_t, 0b11'11'00'00>(t0, t1);
            } else {
                static_assert(!Round, "Invalid round number");
            }
        } else {
            static_assert(!N, "Invalid template parameter N");
        }
    }
}

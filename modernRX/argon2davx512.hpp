#pragma once


/*
* Argon2d AVX512 single round function implementation based on libsodium implementation:
* https://github.com/jedisct1/libsodium/blob/master/src/libsodium/crypto_pwhash/argon2/argon2-fill-block-avx2.c
* https://github.com/jedisct1/libsodium/blob/master/src/libsodium/crypto_pwhash/argon2/blamka-round-avx2.h
*/

#include "avx2.hpp"
#include "avx512.hpp"

namespace modernRX::argon2d {
   // vshuffleepi32 performs right rotation by 32 bits.
   // vshuffleepi8 performs right rotation by 24 or 16 bits accordingly to mask.
   // vpermuteepi64 performs rotation by 64, 128 or 192 bits.

   // Exception from rule to not use macros.
   // Inlining is crucial for performance and i had hard time to make it work just with functions.
#define G21(ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, ymm8, ymm_rot24, ymm_rot16) do {          \
    ml = intrinsics::avx2::vmul<uint64_t>(ymm1, ymm3);                                          \
    ml2 = intrinsics::avx2::vmul<uint64_t>(ymm2, ymm4);                                         \
    ymm1 = intrinsics::avx2::vadd<uint64_t>(ymm1, ymm3);                                        \
    ymm2 = intrinsics::avx2::vadd<uint64_t>(ymm2, ymm4);                                        \
    ymm1 = intrinsics::avx2::vadd<uint64_t>(ymm1, intrinsics::avx2::vadd<uint64_t>(ml, ml));    \
    ymm2 = intrinsics::avx2::vadd<uint64_t>(ymm2, intrinsics::avx2::vadd<uint64_t>(ml2, ml2));  \
                                                                                                \
    ymm7 = intrinsics::avx2::vxor<uint64_t>(ymm7, ymm1);                                        \
    ymm8 = intrinsics::avx2::vxor<uint64_t>(ymm8, ymm2);                                        \
    ymm7 = intrinsics::avx2::vshuffleepi32<uint64_t, 0b10'11'00'01>(ymm7);                      \
    ymm8 = intrinsics::avx2::vshuffleepi32<uint64_t, 0b10'11'00'01>(ymm8);                      \
                                                                                                \
    ml = intrinsics::avx2::vmul<uint64_t>(ymm5, ymm7);                                          \
    ml2 = intrinsics::avx2::vmul<uint64_t>(ymm6, ymm8);                                         \
    ymm5 = intrinsics::avx2::vadd<uint64_t>(ymm5, ymm7);                                        \
    ymm6 = intrinsics::avx2::vadd<uint64_t>(ymm6, ymm8);                                        \
    ymm5 = intrinsics::avx2::vadd<uint64_t>(ymm5, intrinsics::avx2::vadd<uint64_t>(ml, ml));    \
    ymm6 = intrinsics::avx2::vadd<uint64_t>(ymm6, intrinsics::avx2::vadd<uint64_t>(ml2, ml2));  \
                                                                                                \
    ymm3 = intrinsics::avx2::vxor<uint64_t>(ymm3, ymm5);                                        \
    ymm4 = intrinsics::avx2::vxor<uint64_t>(ymm4, ymm6);                                        \
    ymm3 = intrinsics::avx2::vshuffleepi8<uint64_t>(ymm3, ymm_rot24);                           \
    ymm4 = intrinsics::avx2::vshuffleepi8<uint64_t>(ymm4, ymm_rot24);                           \
} while(0);

// Exception from rule to not use macros.
// Inlining is crucial for performance and i had hard time to make it work just with functions.
#define G22(ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, ymm8, ymm_rot24, ymm_rot16) do {          \
    ml = intrinsics::avx2::vmul<uint64_t>(ymm1, ymm3);                                          \
    ml2 = intrinsics::avx2::vmul<uint64_t>(ymm2, ymm4);                                         \
    ymm1 = intrinsics::avx2::vadd<uint64_t>(ymm1, ymm3);                                        \
    ymm2 = intrinsics::avx2::vadd<uint64_t>(ymm2, ymm4);                                        \
    ymm1 = intrinsics::avx2::vadd<uint64_t>(ymm1, intrinsics::avx2::vadd<uint64_t>(ml, ml));    \
    ymm2 = intrinsics::avx2::vadd<uint64_t>(ymm2, intrinsics::avx2::vadd<uint64_t>(ml2, ml2));  \
                                                                                                \
    ymm7 = intrinsics::avx2::vxor<uint64_t>(ymm7, ymm1);                                        \
    ymm8 = intrinsics::avx2::vxor<uint64_t>(ymm8, ymm2);                                        \
    ymm7 = intrinsics::avx2::vshuffleepi8<uint64_t>(ymm7, ymm_rot16);                           \
    ymm8 = intrinsics::avx2::vshuffleepi8<uint64_t>(ymm8, ymm_rot16);                           \
                                                                                                \
    ml = intrinsics::avx2::vmul<uint64_t>(ymm5, ymm7);                                          \
    ml2 = intrinsics::avx2::vmul<uint64_t>(ymm6, ymm8);                                         \
    ymm5 = intrinsics::avx2::vadd<uint64_t>(ymm5, ymm7);                                        \
    ymm6 = intrinsics::avx2::vadd<uint64_t>(ymm6, ymm8);                                        \
    ymm5 = intrinsics::avx2::vadd<uint64_t>(ymm5, intrinsics::avx2::vadd<uint64_t>(ml, ml));    \
    ymm6 = intrinsics::avx2::vadd<uint64_t>(ymm6, intrinsics::avx2::vadd<uint64_t>(ml2, ml2));  \
                                                                                                \
    ymm3 = intrinsics::avx2::vxor<uint64_t>(ymm3, ymm5);                                        \
    ymm4 = intrinsics::avx2::vxor<uint64_t>(ymm4, ymm6);                                        \
    ymm3 = intrinsics::avx512::vprorq<ymm<uint64_t>, 63>(ymm3);                                 \
    ymm4 = intrinsics::avx512::vprorq<ymm<uint64_t>, 63>(ymm4);                                 \
} while(0);

// Exception from rule to not use macros.
// Inlining is crucial for performance and i had hard time to make it work just with functions.
#define ROUND_V1(ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, ymm8, ymm_rot24, ymm_rot16) do {     \
    G21(ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, ymm8, ymm_rot24, ymm_rot16);                  \
    G22(ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, ymm8, ymm_rot24, ymm_rot16);                  \
                                                                                                \
    /*DIAG_V1*/                                                                                 \
    ymm3 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b00'11'10'01>(ymm3);                      \
    ymm4 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b00'11'10'01>(ymm4);                      \
    ymm7 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'01'00'11>(ymm7);                      \
    ymm8 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'01'00'11>(ymm8);                      \
    ymm5 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b01'00'11'10>(ymm5);                      \
    ymm6 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b01'00'11'10>(ymm6);                      \
                                                                                                \
    G21(ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, ymm8, ymm_rot24, ymm_rot16);                  \
    G22(ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, ymm8, ymm_rot24, ymm_rot16);                  \
                                                                                                \
    /*UNDIAG_V1*/                                                                               \
    ymm3 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'01'00'11>(ymm3);                      \
    ymm4 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'01'00'11>(ymm4);                      \
    ymm7 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b00'11'10'01>(ymm7);                      \
    ymm8 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b00'11'10'01>(ymm8);                      \
    ymm5 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b01'00'11'10>(ymm5);                      \
    ymm6 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b01'00'11'10>(ymm6);                      \
} while(0);

// Exception from rule to not use macros.
// Inlining is crucial for performance and i had hard time to make it work just with functions.
#define ROUND_V2(ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, ymm8, ymm_rot24, ymm_rot16) do {     \
    G21(ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, ymm8, ymm_rot24, ymm_rot16);                  \
    G22(ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, ymm8, ymm_rot24, ymm_rot16);                  \
                                                                                                \
    /*DIAG_V2*/                                                                                 \
    auto tmp1{ intrinsics::avx2::vblendepi32<uint64_t, 0b11'00'11'00>(ymm3, ymm4) };            \
    auto tmp2{ intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(ymm3, ymm4) };            \
    ymm4 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'11'00'01>(tmp1);                      \
    ymm3 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'11'00'01>(tmp2);                      \
                                                                                                \
    tmp1 = ymm5;                                                                                \
    ymm5 = ymm6;                                                                                \
    ymm6 = tmp1;                                                                                \
                                                                                                \
    tmp1 = intrinsics::avx2::vblendepi32<uint64_t, 0b11'00'11'00>(ymm7, ymm8);                  \
    tmp2 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(ymm7, ymm8);                  \
    ymm7 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'11'00'01>(tmp1);                      \
    ymm8 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'11'00'01>(tmp2);                      \
                                                                                                \
    G21(ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, ymm8, ymm_rot24, ymm_rot16);                  \
    G22(ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, ymm8, ymm_rot24, ymm_rot16);                  \
                                                                                                \
    /*UNDIAG_V1*/                                                                               \
    tmp1 = intrinsics::avx2::vblendepi32<uint64_t, 0b11'00'11'00>(ymm3, ymm4);                  \
    tmp2 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(ymm3, ymm4);                  \
    ymm3 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'11'00'01>(tmp1);                      \
    ymm4 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'11'00'01>(tmp2);                      \
                                                                                                \
    tmp1 = ymm5;                                                                                \
    ymm5 = ymm6;                                                                                \
    ymm6 = tmp1;                                                                                \
                                                                                                \
    tmp1 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(ymm7, ymm8);                  \
    tmp2 = intrinsics::avx2::vblendepi32<uint64_t, 0b11'00'11'00>(ymm7, ymm8);                  \
    ymm7 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'11'00'01>(tmp1);                      \
    ymm8 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'11'00'01>(tmp2);                      \
} while(0);

// Exception from rule to not use macros.
// Inlining is crucial for performance and i had hard time to make it work just with functions.
#define ROUND_V2WithPrefetch(ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, ymm8, ymm_rot24, ymm_rot16) do {     \
    G21(ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, ymm8, ymm_rot24, ymm_rot16);                              \
    G22(ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, ymm8, ymm_rot24, ymm_rot16);                              \
                                                                                                            \
    /*DIAG_V2*/                                                                                             \
    auto tmp1{ intrinsics::avx2::vblendepi32<uint64_t, 0b11'00'11'00>(ymm3, ymm4) };                        \
    auto tmp2{ intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(ymm3, ymm4) };                        \
    ymm3 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'11'00'01>(tmp2);                                  \
    ymm4 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'11'00'01>(tmp1);                                  \
                                                                                                            \
    tmp1 = intrinsics::avx2::vblendepi32<uint64_t, 0b11'00'11'00>(ymm7, ymm8);                              \
    tmp2 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(ymm7, ymm8);                              \
    ymm7 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'11'00'01>(tmp1);                                  \
    ymm8 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'11'00'01>(tmp2);                                  \
                                                                                                            \
    tmp1 = ymm5;                                                                                            \
    ymm5 = ymm6;                                                                                            \
    ymm6 = tmp1;                                                                                            \
                                                                                                            \
    G21(ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, ymm8, ymm_rot24, ymm_rot16);                              \
    G22WithPrefetch(ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, ymm8, ymm_rot24, ymm_rot16);                  \
                                                                                                            \
    /*UNDIAG_V1*/                                                                                           \
    tmp1 = intrinsics::avx2::vblendepi32<uint64_t, 0b11'00'11'00>(ymm3, ymm4);                              \
    tmp2 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(ymm3, ymm4);                              \
    ymm3 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'11'00'01>(tmp1);                                  \
    ymm4 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'11'00'01>(tmp2);                                  \
                                                                                                            \
    tmp1 = intrinsics::avx2::vblendepi32<uint64_t, 0b00'11'00'11>(ymm7, ymm8);                              \
    tmp2 = intrinsics::avx2::vblendepi32<uint64_t, 0b11'00'11'00>(ymm7, ymm8);                              \
    ymm7 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'11'00'01>(tmp1);                                  \
    ymm8 = intrinsics::avx2::vpermuteepi64<uint64_t, 0b10'11'00'01>(tmp2);                                  \
                                                                                                            \
    tmp1 = ymm5;                                                                                            \
    ymm5 = ymm6;                                                                                            \
    ymm6 = tmp1;                                                                                            \
} while(0);

// Exception from rule to not use macros.
// Inlining is crucial for performance and i had hard time to make it work just with functions.
#define G22WithPrefetch(ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7, ymm8, ymm_rot24, ymm_rot16) do {          \
    ml = intrinsics::avx2::vmul<uint64_t>(ymm1, ymm3);                                                      \
    ymm1 = intrinsics::avx2::vadd<uint64_t>(ymm1, ymm3);                                                    \
    ymm1 = intrinsics::avx2::vadd<uint64_t>(ymm1, intrinsics::avx2::vadd<uint64_t>(ml, ml));                \
    calcRefIndex<XorBlocks>(memory, ctx, tmp_block_ymm[0].m256i_u32[0]);                                    \
    intrinsics::prefetch<PrefetchMode::NTA, 16>(memory[ctx.ref_idx].data());                                \
                                                                                                            \
    ml2 = intrinsics::avx2::vmul<uint64_t>(ymm2, ymm4);                                                     \
    ymm2 = intrinsics::avx2::vadd<uint64_t>(ymm2, ymm4);                                                    \
    ymm2 = intrinsics::avx2::vadd<uint64_t>(ymm2, intrinsics::avx2::vadd<uint64_t>(ml2, ml2));              \
                                                                                                            \
    ymm7 = intrinsics::avx2::vxor<uint64_t>(ymm7, ymm1);                                                    \
    ymm8 = intrinsics::avx2::vxor<uint64_t>(ymm8, ymm2);                                                    \
    ymm7 = intrinsics::avx2::vshuffleepi8<uint64_t>(ymm7, ymm_rot16);                                       \
    ymm8 = intrinsics::avx2::vshuffleepi8<uint64_t>(ymm8, ymm_rot16);                                       \
                                                                                                            \
    ml = intrinsics::avx2::vmul<uint64_t>(ymm5, ymm7);                                                      \
    ml2 = intrinsics::avx2::vmul<uint64_t>(ymm6, ymm8);                                                     \
    ymm5 = intrinsics::avx2::vadd<uint64_t>(ymm5, ymm7);                                                    \
    ymm6 = intrinsics::avx2::vadd<uint64_t>(ymm6, ymm8);                                                    \
    ymm5 = intrinsics::avx2::vadd<uint64_t>(ymm5, intrinsics::avx2::vadd<uint64_t>(ml, ml));                \
    ymm6 = intrinsics::avx2::vadd<uint64_t>(ymm6, intrinsics::avx2::vadd<uint64_t>(ml2, ml2));              \
                                                                                                            \
    ymm3 = intrinsics::avx2::vxor<uint64_t>(ymm3, ymm5);                                                    \
    ymm4 = intrinsics::avx2::vxor<uint64_t>(ymm4, ymm6);                                                    \
    ymm3 = intrinsics::avx512::vprorq<ymm<uint64_t>, 63>(ymm3);                                             \
    ymm4 = intrinsics::avx512::vprorq<ymm<uint64_t>, 63>(ymm4);                                             \
} while(0);
}

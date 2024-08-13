#pragma once


/*
* Argon2d AVX512 single round function implementation based on libsodium implementation:
* https://github.com/jedisct1/libsodium/blob/master/src/libsodium/crypto_pwhash/argon2/argon2-fill-block-avx512f.c
* https://github.com/jedisct1/libsodium/blob/master/src/libsodium/crypto_pwhash/argon2/blamka-round-avx512f.h
*/

#include "avx512.hpp"

namespace modernRX::argon2d {
    namespace {
        inline intrinsics::zmm<uint64_t> muladd(const intrinsics::zmm<uint64_t> x, const intrinsics::zmm<uint64_t> y) noexcept {
            const auto z{ intrinsics::avx512::vpmuludq(x, y) };
            return intrinsics::avx512::vpaddq(intrinsics::avx512::vpaddq(x, y), intrinsics::avx512::vpaddq(z, z));
        }
    }

#define G1(zmmA0, zmmB0, zmmC0, zmmD0, zmmA1, zmmB1, zmmC1, zmmD1)                                                  \
    do {                                                                                                            \
         zmmA0 = muladd(zmmA0, zmmB0);                                                                              \
         zmmA1 = muladd(zmmA1, zmmB1);                                                                              \
                                                                                                                    \
         zmmD0 = intrinsics::avx512::vpxorq(zmmD0, zmmA0);                                                          \
         zmmD1 = intrinsics::avx512::vpxorq(zmmD1, zmmA1);                                                          \
                                                                                                                    \
         zmmD0 = intrinsics::avx512::vprorq<intrinsics::zmm<uint64_t>, 32>(zmmD0);                                  \
         zmmD1 = intrinsics::avx512::vprorq<intrinsics::zmm<uint64_t>, 32>(zmmD1);                                  \
                                                                                                                    \
         zmmC0 = muladd(zmmC0, zmmD0);                                                                              \
         zmmC1 = muladd(zmmC1, zmmD1);                                                                              \
                                                                                                                    \
         zmmB0 = intrinsics::avx512::vpxorq(zmmB0, zmmC0);                                                          \
         zmmB1 = intrinsics::avx512::vpxorq(zmmB1, zmmC1);                                                          \
                                                                                                                    \
         zmmB0 = intrinsics::avx512::vprorq<intrinsics::zmm<uint64_t>, 24>(zmmB0);                                  \
         zmmB1 = intrinsics::avx512::vprorq<intrinsics::zmm<uint64_t>, 24>(zmmB1);                                  \
    } while(0);

#define G2(zmmA0, zmmB0, zmmC0, zmmD0, zmmA1, zmmB1, zmmC1, zmmD1, fetch)                                           \
   do {                                                                                                             \
         zmmA0 = muladd(zmmA0, zmmB0);                                                                              \
         if (fetch) {                                                                                               \
            calcRefIndex<XorBlocks>(memory, ctx, tmp_block_zmm[0].m512i_u32[0]);                                    \
            intrinsics::prefetch<PrefetchMode::NTA, 16>(memory[ctx.ref_idx].data());                                \
         }                                                                                                          \
                                                                                                                    \
        zmmA1 = muladd(zmmA1, zmmB1);                                                                               \
                                                                                                                    \
        zmmD0 = intrinsics::avx512::vpxorq(zmmD0, zmmA0);                                                           \
        zmmD1 = intrinsics::avx512::vpxorq(zmmD1, zmmA1);                                                           \
                                                                                                                    \
        zmmD0 = intrinsics::avx512::vprorq<intrinsics::zmm<uint64_t>, 16>(zmmD0);                                   \
        zmmD1 = intrinsics::avx512::vprorq<intrinsics::zmm<uint64_t>, 16>(zmmD1);                                   \
                                                                                                                    \
        zmmC0 = muladd(zmmC0, zmmD0);                                                                               \
        zmmC1 = muladd(zmmC1, zmmD1);                                                                               \
                                                                                                                    \
        zmmB0 = intrinsics::avx512::vpxorq(zmmB0, zmmC0);                                                           \
        zmmB1 = intrinsics::avx512::vpxorq(zmmB1, zmmC1);                                                           \
                                                                                                                    \
        zmmB0 = intrinsics::avx512::vprorq<intrinsics::zmm<uint64_t>, 63>(zmmB0);                                   \
        zmmB1 = intrinsics::avx512::vprorq<intrinsics::zmm<uint64_t>, 63>(zmmB1);                                   \
    } while(0);

#define DIAGONALIZE(zmmA0, zmmB0, zmmC0, zmmD0, zmmA1, zmmB1, zmmC1, zmmD1)                                         \
    do {                                                                                                            \
        zmmB0 = intrinsics::avx512::vpermq<intrinsics::zmm<uint64_t>, 0x39>(zmmB0);                                 \
        zmmB1 = intrinsics::avx512::vpermq<intrinsics::zmm<uint64_t>, 0x39>(zmmB1);                                 \
                                                                                                                    \
        zmmC0 = intrinsics::avx512::vpermq<intrinsics::zmm<uint64_t>, 0x4e>(zmmC0);                                 \
        zmmC1 = intrinsics::avx512::vpermq<intrinsics::zmm<uint64_t>, 0x4e>(zmmC1);                                 \
                                                                                                                    \
        zmmD0 = intrinsics::avx512::vpermq<intrinsics::zmm<uint64_t>, 0x93>(zmmD0);                                 \
        zmmD1 = intrinsics::avx512::vpermq<intrinsics::zmm<uint64_t>, 0x93>(zmmD1);                                 \
    } while(0);

#define UNDIAGONALIZE(zmmA0, zmmB0, zmmC0, zmmD0, zmmA1, zmmB1, zmmC1, zmmD1)                                       \
    do {                                                                                                            \
        zmmB0 = intrinsics::avx512::vpermq<intrinsics::zmm<uint64_t>, 0x93>(zmmB0);                                 \
        zmmB1 = intrinsics::avx512::vpermq<intrinsics::zmm<uint64_t>, 0x93>(zmmB1);                                 \
                                                                                                                    \
        zmmC0 = intrinsics::avx512::vpermq<intrinsics::zmm<uint64_t>, 0x4e>(zmmC0);                                 \
        zmmC1 = intrinsics::avx512::vpermq<intrinsics::zmm<uint64_t>, 0x4e>(zmmC1);                                 \
                                                                                                                    \
        zmmD0 = intrinsics::avx512::vpermq<intrinsics::zmm<uint64_t>, 0x39>(zmmD0);                                 \
        zmmD1 = intrinsics::avx512::vpermq<intrinsics::zmm<uint64_t>, 0x39>(zmmD1);                                 \
    } while(0);

#define SWAP_HALVES(zmm0, zmm1)                                                                                     \
    do {                                                                                                            \
        const __m512i t = intrinsics::avx512::vshufi64x2<intrinsics::zmm<uint64_t>, 0x44>(zmm0, zmm1);              \
        zmm1 = intrinsics::avx512::vshufi64x2<intrinsics::zmm<uint64_t>, 0xee>(zmm0, zmm1);                         \
        zmm0 = t;                                                                                                   \
    } while (0);

#define SWAP_QUARTERS(zmmA0, zmmA1)                                                                                 \
    do {                                                                                                            \
        SWAP_HALVES(zmmA0, zmmA1);                                                                                  \
        zmmA0 = intrinsics::avx512::vshufi64x2<intrinsics::zmm<uint64_t>, 0xd8>(zmmA0, zmmA0);                      \
        zmmA1 = intrinsics::avx512::vshufi64x2<intrinsics::zmm<uint64_t>, 0xd8>(zmmA1, zmmA1);                      \
    } while(0);

#define UNSWAP_QUARTERS(zmmA0, zmmA1)                                                                               \
    do {                                                                                                            \
        zmmA0 = intrinsics::avx512::vshufi64x2<intrinsics::zmm<uint64_t>, 0xd8>(zmmA0, zmmA0);                      \
        zmmA1 = intrinsics::avx512::vshufi64x2<intrinsics::zmm<uint64_t>, 0xd8>(zmmA1, zmmA1);                      \
        SWAP_HALVES(zmmA0, zmmA1);                                                                                  \
    } while(0);


#define ROUND(zmmA0, zmmB0, zmmC0, zmmD0, zmmA1, zmmB1, zmmC1, zmmD1, fetch)                                        \
    do {                                                                                                            \
        G1(zmmA0, zmmB0, zmmC0, zmmD0, zmmA1, zmmB1, zmmC1, zmmD1);                                                 \
        G2(zmmA0, zmmB0, zmmC0, zmmD0, zmmA1, zmmB1, zmmC1, zmmD1, false);                                          \
                                                                                                                    \
        DIAGONALIZE(zmmA0, zmmB0, zmmC0, zmmD0, zmmA1, zmmB1, zmmC1, zmmD1);                                        \
                                                                                                                    \
        G1(zmmA0, zmmB0, zmmC0, zmmD0, zmmA1, zmmB1, zmmC1, zmmD1);                                                 \
        G2(zmmA0, zmmB0, zmmC0, zmmD0, zmmA1, zmmB1, zmmC1, zmmD1, fetch);                                          \
                                                                                                                    \
        UNDIAGONALIZE(zmmA0, zmmB0, zmmC0, zmmD0, zmmA1, zmmB1, zmmC1, zmmD1);                                      \
    } while(0);

#define ROUND_V1(zmmA0, zmmC0, zmmB0, zmmD0, zmmA1, zmmC1, zmmB1, zmmD1)                                            \
    do {                                                                                                            \
        SWAP_HALVES(zmmA0, zmmB0);                                                                                  \
        SWAP_HALVES(zmmC0, zmmD0);                                                                                  \
        SWAP_HALVES(zmmA1, zmmB1);                                                                                  \
        SWAP_HALVES(zmmC1, zmmD1);                                                                                  \
        ROUND(zmmA0, zmmB0, zmmC0, zmmD0, zmmA1, zmmB1, zmmC1, zmmD1, false);                                       \
        SWAP_HALVES(zmmA0, zmmB0);                                                                                  \
        SWAP_HALVES(zmmC0, zmmD0);                                                                                  \
        SWAP_HALVES(zmmA1, zmmB1);                                                                                  \
        SWAP_HALVES(zmmC1, zmmD1);                                                                                  \
    } while(0);

#define ROUND_V2(zmmA0, zmmA1, zmmB0, zmmB1, zmmC0, zmmC1, zmmD0, zmmD1, fetch)                                     \
    do {                                                                                                            \
        SWAP_QUARTERS(zmmA0, zmmA1);                                                                                \
        SWAP_QUARTERS(zmmB0, zmmB1);                                                                                \
        SWAP_QUARTERS(zmmC0, zmmC1);                                                                                \
        SWAP_QUARTERS(zmmD0, zmmD1);                                                                                \
        ROUND(zmmA0, zmmB0, zmmC0, zmmD0, zmmA1, zmmB1, zmmC1, zmmD1, fetch);                                       \
        UNSWAP_QUARTERS(zmmA0, zmmA1);                                                                              \
        UNSWAP_QUARTERS(zmmB0, zmmB1);                                                                              \
        UNSWAP_QUARTERS(zmmC0, zmmC1);                                                                              \
        UNSWAP_QUARTERS(zmmD0, zmmD1);                                                                              \
    } while(0);
}

#pragma once

/*
* Wrapper over some AVX512 intrinsics for speeding up some RandomX algorithm parts.
* Used, but not defined by RandomX algorithm.
* Code may be a little bit messy and not fully documented as this will be further extended in unknown direction.
*/

#include <array>

#include "intrinsics.hpp"

namespace modernRX::intrinsics::avx512 {
   template<typename V, int imm>
   [[nodiscard]] constexpr V vprorq(const V x) noexcept {
      if constexpr (std::is_same_v<V, ymm<uint64_t>>) {
         return _mm256_ror_epi64(x, imm);
      } else if constexpr (std::is_same_v<V, zmm<uint64_t>>) {
         return _mm512_ror_epi64(x, imm);
      } else {
         static_assert(!sizeof(V), "the only supported type for this operation is: 256/512b vector of uint64");
      }
   }

   template<typename V, int imm>
   [[nodiscard]] constexpr V vpermq(const V x) noexcept {
       if constexpr (std::is_same_v<V, zmm<uint64_t>>) {
           return _mm512_permutex_epi64(x, imm);
       }
       else {
           static_assert(!sizeof(V), "the only supported type for this operation is: 512b vector of uint64");
       }
   }

   template<typename V, int imm>
   [[nodiscard]] constexpr V vshufi64x2(const V x, const V y) noexcept {
       if constexpr (std::is_same_v<V, zmm<uint64_t>>) {
           return _mm512_shuffle_i64x2(x, y, imm);
       }
       else {
           static_assert(!sizeof(V), "the only supported type for this operation is: 512b vector of uint64");
       }
   }

   [[nodiscard]] inline zmm<uint64_t> vpxorq(const zmm<uint64_t> x, const zmm<uint64_t> y) noexcept {
      return _mm512_xor_epi64(x, y);
   }

   [[nodiscard]] inline zmm<uint64_t> vpmuludq(const zmm<uint64_t> x, const zmm<uint64_t> y) noexcept {
       return _mm512_mul_epu32(x, y);
   }

   [[nodiscard]] inline zmm<uint64_t> vpaddq(const zmm<uint64_t> x, const zmm<uint64_t> y) noexcept {
       return _mm512_add_epi64(x, y);
   }
}

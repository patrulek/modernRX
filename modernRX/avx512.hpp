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
      }
      else {
         static_assert(!sizeof(V), "the only supported type for this operation is: 256b vector of uint64");
      }
   }
}

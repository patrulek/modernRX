#pragma once

/*
* Commons for AES operations. Encode/decode is implemented with hardware specific instructions.
*/

#include <span>

#include "aliases.hpp"
#include "intrinsics.hpp"

namespace modernRX::intrinsics::aes {

    // Performs AES encoding of an input state with given key.
    // Overwrites input state with encoding result.
    inline void encode(xmm128i_t& state, const xmm128i_t key) noexcept {
        state = _mm_aesenc_si128(state, key);
    }

    // Performs AES decoding of an input state with given key.
    // Overwrites input state with decoding result.
    inline void decode(xmm128i_t& state, const xmm128i_t key) noexcept {
        state = _mm_aesdec_si128(state, key);
    }
}

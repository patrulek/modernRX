#pragma once

/*
* Commons for AES operations. Encode/decode is implemented without hardware specific instructions.
*/

#include <span>

#include "aliases.hpp"

namespace modernRX::aes {
	// Performs AES encoding of an input state with given key.
	// Overwrites input state with encoding result.
	void encode(std::span<uint32_t, 4> state, const_span<uint32_t, 4> key);

	// Performs AES decoding of an input state with given key.
	// Overwrites input state with decoding result.
	void decode(std::span<uint32_t, 4> state, const_span<uint32_t, 4> key);
}
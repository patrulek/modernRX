#pragma once

/*
* Blake2b pseudo random number generator: https://github.com/tevador/RandomX/blob/master/doc/specs.md#35-blakegenerator
* This is used for RandomX superscalar program generation.
*/

#include "blake2b.hpp"

namespace modernRX::blake2b {
	// Holds generator's current state (which is 64-bytes blake2b hash) and allows to get 8- or 32-bit values.
	class Random {
	public:
		// Calculates blake2b hash from given seed and nonce and set it as a current state.
		// Seed max size is 60 bytes (bigger buffers will be truncated).
		[[nodiscard]] explicit Random(const_span<std::byte> seed, const uint32_t nonce = 0);

		// Return 8-bit unsigned int. Rehashes state if no bytes available. 
		[[nodiscard]] uint8_t getUint8();

		// Return 32-bit unsigned int. Rehashes state if less than 4 bytes available.
		[[nodiscard]] uint32_t getUint32();
	private:
		// Generates new state (blake2b hash) if left less bytes than needed.
		void rehashIfNeeded(const size_t bytes_needed);

		std::array<std::byte, Max_Digest_Size> state{};
		uint32_t position{ 0 };
	};
}

#pragma once

/*
* Blake2b pseudo random number generator.
* This is used for RandomX superscalar program generation.
*/

#include "blake2b.hpp"
#include "utils.hpp"

namespace modernRX::blake2b {
	// Holds generator's current state (which is 64-bytes blake2b hash) and allows to get 8- or 32-bit values.
	class Random {
	public:
		// Calculates blake2b hash from given seed and nonce and set it as a current state.
		// Seed max size is 60 bytes (bigger buffers will be truncated).
		explicit Random(const std::span<std::byte> seed, const uint32_t nonce);

		// Return 8-bit unsigned int. Rehashes state if no bytes available. 
		uint8_t getUint8();

		// Return 32-bit unsigned int. Rehashes state if less than 4 bytes available.
		uint32_t getUint32();
	private:
		// Generates new state (blake2b hash) if left less bytes than needed.
		void rehashIfNeeded(const size_t bytes_needed);

		std::array<std::byte, Max_Digest_Size> state{};
		uint32_t position{ 0 };
	};
}
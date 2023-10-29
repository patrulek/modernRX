#pragma once

/*
* Single-threaded and no SIMD supported implementation of Blake2b based on: https://datatracker.ietf.org/doc/html/rfc7693 and https://github.com/tevador/RandomX
* This is used by Argon2d algorithm, Blake2bRandom, Aes4RGenerator and by the RandomX algorithm to calculate final hash.
*/

#include <array>
#include <span>

#include "aliases.hpp"

namespace modernRX::blake2b {
	inline constexpr uint32_t Block_Size{ 128 }; // In bytes. Not fully filled blocks are padded with zeros.
	inline constexpr std::array<uint32_t, 4> Rotation_Constants{ 32, 24, 16, 63 }; // Rotation constants for G function.

	// Initialization vector.
	inline constexpr std::array<uint64_t, 8> IV{
		0x6a09e667f3bcc908, 0xbb67ae8584caa73b, 0x3c6ef372fe94f82b, 0xa54ff53a5f1d36f1,
		0x510e527fade682d1, 0x9b05688c2b3e6c1f, 0x1f83d9abfb41bd6b, 0x5be0cd19137e2179,
	};

	inline constexpr uint32_t Max_Digest_Size{ 64 }; // In bytes. Must be equal to size of initialization vector.
	static_assert(Max_Digest_Size == sizeof(IV));

	// Input data permutations.
	inline constexpr std::array<std::array<uint32_t, 16>, 12> Sigma{
		std::array<uint32_t, 16>{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
		std::array<uint32_t, 16>{ 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
		std::array<uint32_t, 16>{ 11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4 },
		std::array<uint32_t, 16>{ 7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8 },
		std::array<uint32_t, 16>{ 9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13 },
		std::array<uint32_t, 16>{ 2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9 },
		std::array<uint32_t, 16>{ 12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11 },
		std::array<uint32_t, 16>{ 13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10 },
		std::array<uint32_t, 16>{ 6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5 },
		std::array<uint32_t, 16>{ 10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0 },
		std::array<uint32_t, 16>{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
		std::array<uint32_t, 16>{ 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
	};

	/* 
	* Uses given input data and key and stores Blake2b hash in output parameter.
	*
	* Output's size is simultaneously a digest size, so it must be in blake2b valid digest range(1-64), otherwise it will throw.
	* 
	* Input may be any size between 0 and size_t max. This is a bit limiting comparing to specification (original implementation allows input buffer
	* be up to 2^128 in size; this function allows only up to 2^64), but its ok because RandomX doesnt need so big inputs to be hashed.
	* Input may point to the same buffer as output.
	*
	* Key must be in blake2b valid key size range(0-64), otherwise it will throw.
	*/
	void hash(std::span<std::byte> output, const_span<std::byte> input, const_span<std::byte> key);

	// The content of this namespace should be internal, but Argon2d implementation relies on these.
	inline namespace internal {
		// Holds current state of blake2b algorithm.
		struct Context {
			std::array<std::byte, Block_Size> block{};	// Block buffer to compress.
			std::array<uint64_t, 8> state{ IV };		// Chained state that will yield hash.
			std::array<uint64_t, 2> counters{ 0, 0 };	// Total number of processed bytes.
			size_t block_idx{ 0 };						// Current position in block buffer.
			size_t digest_size{ 0 };                    // Output size.
		};

		// Initializes blake2b state. If key size > 0, treat it as a first block and compress.
		[[nodiscard]] Context init(const_span<std::byte> key, const uint32_t digest_size) noexcept;

		// Fills block buffer with input and compress all fully filled blocks.
		void update(Context& ctx, const_span<std::byte> input) noexcept;
		
		// Compress last block and generates final state that is used to yield a blake2b hash.
		void final(std::span<std::byte> hash, Context& ctx) noexcept;
	}
};

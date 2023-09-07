#pragma once

/*
* Single-threaded, AVX2 supported and RandomX-specialized implementation of Argon2d based on: https://github.com/P-H-C/phc-winner-argon2
* Implementation does not contain calculating final hash, this only provide memory filling step.
* This is used to fill RandomX cache memory.
*/

#include <array>
#include <span>
#include <vector>

#include "aliases.hpp"
#include "randomxparams.hpp"

namespace modernRX::argon2d {
	namespace blake2b {
		/*
		* Calculates variable-length (not limited to 64 bytes) blake2b hashes (refers to 3.2 in https://github.com/P-H-C/phc-winner-argon2/blob/master/argon2-specs.pdf)
		* RandomX uses this function in several places with fixed output and input sizes, thus some assumptions were made to optimize this function.
		*   - Output's size is simultaneously a digest size and it is always 1024 bytes.
		*   - Input size is always 72 bytes.
		*
		* Input may point to the same buffer as output.
		*/
		void hash(std::span<std::byte> output, const_span<std::byte> input) noexcept;
	}

	inline constexpr uint32_t Block_Size{ 1024 }; // Memory block size in bytes.
	inline constexpr uint32_t Memory_Size{ Rx_Argon2d_Memory_Blocks * Block_Size }; // Memory size in bytes. This value is fixed for all possible argon2d::Memory objects.

	using Block = std::array<std::byte, Block_Size> alignas(64);
	using Memory = std::vector<Block>;

	// Performs Argon2d algorithm to fill given memory blocks according to given input data and params.
	//
	// Memory is the output parameter and its size is always equal to Rx_Argon2d_Memory_Blocks.
	//
	// Password is an input parameter of any size.
	void fillMemory(Memory& memory, const_span<std::byte> password) noexcept;
}
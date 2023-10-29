#pragma once

/*
* Single-threaded and no SIMD supported implementation of Argon2d based on: https://github.com/P-H-C/phc-winner-argon2
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
		// Calculates variable-length (not limited to 64 bytes) blake2b hashes (refers to 3.2 in https://github.com/P-H-C/phc-winner-argon2/blob/master/argon2-specs.pdf)
		//
		// Output's size is produced digest size and must be greater than 0, otherwise function will throw.
		// 
		// Input size must be in valid range according to specs (from 1 to 2^32 - 1 bytes), otherwise function will throw.
		// Input may be the same buffer as output.
		void hash(std::span<std::byte> output, const_span<std::byte> input);
	}

	inline constexpr uint32_t Block_Size{ 1024 }; // Memory block size in bytes.

	using Block = std::array<std::byte, Block_Size>;
	using Memory = std::vector<Block>;

	// Performs Argon2d algorithm to fill given memory blocks according to given input data and params.
	//
	// Memory is the output parameter and its size must be equal to Rx_Argon2d_Memory_Blocks, otherwise function will throw.
	//
	// Password is an input parameter of any size.
	//
	// Salt is an input parameter and its size must be greater-equal than 8, otherwise function will throw.
	void fillMemory(Memory& memory, const_span<std::byte> password, const_span<std::byte> salt);
}

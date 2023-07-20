#pragma once

/*
* Single-threaded and no avx supported implementation of Argon2d based on: https://github.com/P-H-C/phc-winner-argon2
* Implementation does not contain calculating final hash, this only provide memory filling step.
* This is used to fill RandomX cache memory.
*/

#include <array>
#include <span>
#include <vector>

namespace modernRX::argon2d {
	namespace blake2b {
		// Calculates variable-length (not limited to 64 bytes) blake2b hashes (refers to 3.2 in https://github.com/P-H-C/phc-winner-argon2/blob/master/argon2-specs.pdf)
		//
		// Output's size is produced digest size and must be greater than 0.
		// 
		// Input size must be in valid range according to specs (up to 2^32 - 1 bytes).
		// Input may be the same buffer as output.
		void hash(std::span<std::byte> output, const std::span<std::byte> input);
	}

	inline constexpr uint32_t Min_Memory_Blocks = 8; // Number of memory blocks to fill. Multiplied by block size will give total amount of memory to fill.
	inline constexpr uint32_t Max_Parallelism = 16'777'215; // Maximum number of lanes. 

	inline constexpr uint32_t Sync_Points = 4; // Number of equally long lane slices. End of slice can be treated as synchronization point.
	inline constexpr uint32_t Block_Size = 1024; // In bytes.
	inline constexpr uint32_t Uint64_Per_Block = Block_Size / sizeof(uint64_t); // Number of uint64 elements per memory block.

	// Holds algorithm configuration parameters.
	struct Params {
		std::span<std::byte> secret;			// Serves as key. Size must be in range (0 .. 2^32 - 1).
		std::span<std::byte> data;				// Associated data. Size must be in range (0 .. 2^32 - 1).
		uint32_t parallelism;					// Number of lanes/threads used to fill memory. Must be in range (1 .. 2^24 - 1).
		uint32_t tag_length;					// Digest size. Must be in range (4 .. 2^32 - 1).
		uint32_t memory_blocks;					// Number of memory blocks. Defines total amount of memory to fill. Must be in range (8 * parallelism .. 2^32 - 1)
		uint32_t iterations;					// Number of iterations. Must be in range (1 .. 2^32 - 1).
		uint32_t type{ 0 };						// Enumeration for Argon2 algorithm type. For Argon2d this must be 0.
		uint32_t version{ 0x13 };				// Version of Argon2d impementation. Must be 0x13.
	};


	using Block = std::array<std::byte, Block_Size>;
	using Memory = std::vector<Block>;

	// Performs Argon2d algorithm to fill given memory blocks according to given input data and params.
	//
	// Memory is the output parameter and its size must be equal to params.memory_blocks, otherwise it will throw.
	//
	// Password is an input parameter and its size must be in range (0 .. 2^32 - 1).
	//
	// Salt is an input parameter and its size must be in range (8 .. 2^32 - 1).
	//
	// Params is an input parameter that configures Argon2d algorithm.
	void fillMemory(Memory& memory, const std::span<std::byte> password, const std::span<std::byte> salt, const Params& params);
}
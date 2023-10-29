#pragma once

/*
* Single-threaded and no hardware supported implementation of AES pseudo random generator defined by https://github.com/tevador/RandomX/blob/master/doc/specs.md#33-aesgenerator4r.
* This is used by RandomX algorithm to generate random programs.
*/

#include <array>
#include <span>

namespace modernRX::aes {
	// Fills output with 4-round, 4x128 aes generated hashes.
	// Single iteration (four rounds) produces 64-bytes of data.
	//
	// Output's size must be multiply of 64, otherwise it will throw.
	// Seed is an initial state of the generator and may change during the execution.
	void fill4R(std::span<std::byte> output, std::span<std::byte, 64> seed);
}

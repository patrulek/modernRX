#pragma once

/*
* Single-threaded and no hardware supported implementation of AES pseudo random generator defined by https://github.com/tevador/RandomX/blob/master/doc/specs.md#33-aesgenerator4r.
* This is used by RandomX algorithm to generate random programs.
*/

#include <array>
#include <span>

#include "aes.hpp"

namespace modernRX::aes {

	class Random4R {
	public:
		// Initializes internal state with given seed.
		explicit Random4R(const std::span<std::byte, 64> seed);

		// Fills output with 4-round, 4x128 aes generated hashes.
		// Single iteration (four rounds) produces 64-bytes of data.
		//
		// Output's size must be multiply of 64, otherwise it will throw.
		void fill(std::span<std::byte> output);

		std::byte* data();
	private:
		std::array<std::array<uint32_t, 4>, 4> state{};
	};
}
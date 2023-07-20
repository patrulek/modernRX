#pragma once

/*
* Single-threaded and no hardware supported implementation of AES hash function defined by https://github.com/tevador/RandomX/blob/master/doc/specs.md#34-aeshash1r.
* This is used by RandomX algorithm calculate fingerprint of a VM's Scratchpad memory.
*/

#include <span>

namespace modernRX::aes {
	// Calculates a 64-byte fingerprint of its input.
	void hash1R(std::span<std::byte, 64> output, const std::span<std::byte> input);
}
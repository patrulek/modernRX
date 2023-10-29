#pragma once

/*
* Single-threaded and no hardware supported implementation of AES hash function defined by https://github.com/tevador/RandomX/blob/master/doc/specs.md#34-aeshash1r.
* This is used by RandomX algorithm calculate fingerprint of a VM's Scratchpad memory.
*/

#include <span>

#include "aliases.hpp"

namespace modernRX::aes {
    // Calculates a 64-byte fingerprint of its input.
    // The input size must be a multiple of 64 bytes.
    void hash1R(std::span<std::byte, 64> output, const_span<std::byte> input) noexcept;
}

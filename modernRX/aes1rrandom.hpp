#pragma once

/*
* Single-threaded and hardware supported implementation of AES pseudo random generator defined by https://github.com/tevador/RandomX/blob/master/doc/specs.md#32-aesgenerator1r.
* This is used by RandomX algorithm to fill VM Scratchpad memory and to initialize AesGenerator4R.
*/

#include <array>
#include <span>

namespace modernRX::aes {
    // Fills output with 1-round, 4x128 aes generated hashes.
    // Single round produces 64-bytes of data.
    //
    // Output's size must be multiply of 64 if Fixed is false, otherwise it will be equal to Scrachpad memory size.
    // Seed is an initial state of the generator and will change to last 64-bytes of output after the function call.
    template<bool Fixed = true>
    void fill1R(std::span<std::byte> output, std::span<std::byte, 64> seed) noexcept;
}

#pragma once

/*
* Defines helper function that returns reciprocal values.
* Used by RandomX algorithm.
*/

#include <bit>

#include "assertume.hpp"

// Calculates reciprocal = 2**x / divisor for highest integer x such that reciprocal < 2**64.
// divisor must not be 0 or a power of 2
// 
// Equivalent x86 assembly (divisor in rcx):
// 
// mov edx, 1
// mov r8, rcx
// xor eax, eax
// bsr rcx, rcx
// shl rdx, cl
// div r8
// ret
[[nodiscard]] constexpr uint64_t reciprocal(const uint32_t divisor) noexcept {
    // Divisor will never be 0 or a power of 2.
    ASSERTUME(divisor != 0 && !std::has_single_bit(divisor));

    constexpr uint64_t p2exp63{ 1ULL << 63 };
    const uint64_t quotient{ p2exp63 / divisor };
    const uint64_t remainder{ p2exp63 % divisor };

    uint32_t shift{ 32 };
    for (uint32_t k = 1U << 31; (k & divisor) == 0; k >>= 1) {
        --shift;
    }

    return (quotient << shift) + ((remainder << shift) / divisor);
}

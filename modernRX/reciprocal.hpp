#pragma once

/*
* Defines helper function that returns reciprocal values.
* Used by RandomX algorithm.
*/

#include <bit>

#include "assertume.hpp"

//	Calculates reciprocal = 2**x / divisor for highest integer x such that reciprocal < 2**64.
//	divisor must not be 0 or a power of 2
//
//	Equivalent x86 assembly (divisor in rcx):
//
//	mov edx, 1
//	mov r8, rcx
//	xor eax, eax
//	bsr rcx, rcx
//	shl rdx, cl
//	vdiv r8
//	ret
constexpr uint64_t reciprocal(const uint64_t divisor) noexcept {
	// Divisor will never be 0 or a power of 2.
	ASSERTUME(!std::has_single_bit(divisor));

	constexpr uint64_t p2exp63{ 1ULL << 63 };
	uint64_t quotient{ p2exp63 / divisor };
	uint64_t remainder{ p2exp63 % divisor };

	uint32_t bsr{ 0 }; // Highest set bit in divisor.

	for (uint64_t bit = divisor; bit > 0; bit >>= 1) {
		++bsr;
	}

	for (uint32_t shift = 0; shift < bsr; ++shift) {
		if (remainder >= divisor - remainder) {
			quotient = quotient * 2 + 1;
			remainder = remainder * 2 - divisor;
			continue;
		}

		quotient = quotient * 2;
		remainder = remainder * 2;
	}

	return quotient;
}
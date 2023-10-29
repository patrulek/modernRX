#pragma once

/*
* Wrapper over compiler intrinsics.
* Used by RandomX algorithm.
*/

#include <intrin.h>

namespace modernRX::intrinsics {
	[[nodiscard]] inline uint64_t smulh(const int64_t a, const int64_t b) noexcept {
		int64_t hi;
		_mul128(a, b, &hi);
		return hi;
	}

	[[nodiscard]] inline uint64_t umulh(const uint64_t a, const uint64_t b) noexcept {
		return __umulh(a, b);
	}
}

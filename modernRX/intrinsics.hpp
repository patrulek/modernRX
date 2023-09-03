#pragma once

/*
* Wrapper over compiler intrinsics.
* Used, but not defined by RandomX algorithm.
*/

#include <intrin.h>

namespace modernRX::intrinsics {
	inline uint64_t smulh(const int64_t a, const int64_t b) {
		int64_t hi;
		_mul128(a, b, &hi);
		return hi;
	}

	inline uint64_t umulh(const uint64_t a, const uint64_t b) {
		return __umulh(a, b);
	}
}
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

	enum class PrefetchMode : uint8_t {
		T0 = _MM_HINT_T0,
		T1 = _MM_HINT_T1,
		T2 = _MM_HINT_T2,
		NTA = _MM_HINT_NTA,
	};

	// Prefetches a cache line and reinterprets given pointer to the specified type.
	template<PrefetchMode Mode = PrefetchMode::NTA, typename Ret = void>
	inline Ret prefetch(const void* ptr) {
        _mm_prefetch(reinterpret_cast<const char*>(ptr), static_cast<int>(Mode));
		if constexpr (!std::is_void_v<Ret>) {
			return reinterpret_cast<Ret>(ptr);
		}
    }
}
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

    enum class PrefetchMode : uint8_t {
        T0 = _MM_HINT_T0,
        T1 = _MM_HINT_T1,
        T2 = _MM_HINT_T2,
        NTA = _MM_HINT_NTA,
    };

    // Prefetches a cache line and reinterprets given pointer to the specified type.
    template<PrefetchMode Mode = PrefetchMode::NTA, typename Ret = void>
    [[nodiscard]] inline Ret prefetch(const void* ptr) noexcept {
        _mm_prefetch(reinterpret_cast<const char*>(ptr), static_cast<int>(Mode));
        if constexpr (!std::is_void_v<Ret>) {
            return reinterpret_cast<Ret>(ptr);
        }
    }

    // Prefetch a CacheLines number of cache lines.
    template<PrefetchMode Mode = PrefetchMode::NTA, size_t CacheLines = 1>
    inline void prefetch(const void* ptr) noexcept {
        for (size_t i = 0; i < CacheLines; ++i) {
            _mm_prefetch(reinterpret_cast<const char*>(ptr) + i * 64, static_cast<int>(Mode));
        }
    }
}

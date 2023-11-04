#pragma once

/*
* Wrapper over compiler intrinsics.
* Used by RandomX algorithm.
*/

#include <intrin.h>

namespace modernRX::intrinsics {
    template<typename T>
    struct xmm_wrapper {
        using type = std::conditional_t<std::is_integral_v<T>, __m128i,
                     std::conditional_t<std::is_same_v<T, double>, __m128d, void>>;

        static_assert(!std::is_same_v<type, void>, "type can be one of integral types or double");
    };

    template<typename T>
    using xmm = xmm_wrapper<T>::type;
    using xmm128i_t = xmm<int>;
    using xmm128d_t = xmm<double>;

    template<typename... Args>
    [[nodiscard]] constexpr xmm128i_t fromChars(const Args&&... chars) noexcept {
        static_assert(sizeof...(chars) == 16, "must be 16 chars");
        return xmm128i_t{ static_cast<const char>(chars)... };
    }

    template<typename T>
    struct ymm_wrapper {
        using type = std::conditional_t<std::is_integral_v<T>, __m256i,
            std::conditional_t<std::is_same_v<T, double>, __m256d,
            std::conditional_t<std::is_same_v<T, float>, __m256, void>>>;

        static_assert(!std::is_same_v<type, void>, "type can be one of integral types, double or float");
    };

    template<typename T>
    using ymm = ymm_wrapper<T>::type;

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

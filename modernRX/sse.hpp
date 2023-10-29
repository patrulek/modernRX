#pragma once

/*
* Wrapper over some SSE intrinsics required by RandomX algorithm.
* Implements FloatEnvironment RAII class to set and reset float environment, which is required by RandomX algorithm.
* Used by RandomX algorithm.
* Code may be a little bit messy and not fully documented as this will be further extended in unknown direction.
*/

#include <array>
#include <bit>
#include <cfenv>
#include <emmintrin.h>

namespace modernRX::intrinsics::sse {
    inline constexpr uint32_t Rx_Mxcsr_Default{ 0x9FC0 }; //Flush to zero, denormals are zero, default rounding mode, all exceptions disabled.
    inline constexpr uint32_t Floating_Round_Modes{ 4 };

    // Sets float environment and restore it on destruction.
    class FloatEnvironment {
    public:
        // Swaps current float environment with provided one.
        [[nodiscard]] explicit FloatEnvironment(const uint32_t csr = Rx_Mxcsr_Default) noexcept {
            fegetenv(&fenv);
            _mm_setcsr(csr); // Reset float state.
        }
        ~FloatEnvironment() {
            fesetenv(&fenv);
        }
        FloatEnvironment(const FloatEnvironment&) = delete;
        FloatEnvironment(const FloatEnvironment&&) = delete;
        FloatEnvironment& operator=(const FloatEnvironment&) = delete;
        FloatEnvironment& operator=(const FloatEnvironment&&) = delete;
    private:
        fenv_t fenv;
    };

    // Modifies environment's rounding mode.
    // Mode limited to 4 values with modulo operation.
    inline void setFloatRoundingMode(const uint32_t mode, const uint32_t csr = Rx_Mxcsr_Default) noexcept {
        _mm_setcsr(csr | (mode << 13));
    }

    template<typename T>
    struct xmm_wrapper {
        using type = std::conditional_t<std::is_same_v<T, double>, __m128d, void>;

        static_assert(!std::is_same_v<type, void>, "type must be double");
    };

    template<typename T>
    using xmm = xmm_wrapper<T>::type;

    template<typename T>
    [[nodiscard]] constexpr xmm<T> vxor(const xmm<T> x, const xmm<T> y) noexcept {
        if constexpr (std::is_same_v<T, double>) {
            return _mm_xor_pd(x, y);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: float64");
        }
    }

    template<typename T>
    [[nodiscard]] constexpr xmm<T> vand(const xmm<T> x, const xmm<T> y) noexcept {
        if constexpr (std::is_same_v<T, double>) {
            return _mm_and_pd(x, y);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: float64");
        }
    }


    template<typename T>
    [[nodiscard]] constexpr xmm<T> vor(const xmm<T> x, const xmm<T> y) noexcept {
        if constexpr (std::is_same_v<T, double>) {
            return _mm_or_pd(x, y);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: float64");
        }
    }

    template<typename T>
    [[nodiscard]] constexpr xmm<T> vadd(const xmm<T> x, const xmm<T> y) noexcept {
        if constexpr (std::is_same_v<T, double>) {
            return _mm_add_pd(x, y);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: float64");
        }
    }

    template<typename T>
    [[nodiscard]] constexpr xmm<T> vsub(const xmm<T> x, const xmm<T> y) noexcept {
        if constexpr (std::is_same_v<T, double>) {
            return _mm_sub_pd(x, y);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: float64");
        }
    }

    template<typename T>
    [[nodiscard]] constexpr xmm<T> vmul(const xmm<T> x, const xmm<T> y) noexcept {
        if constexpr (std::is_same_v<T, double>) {
            return _mm_mul_pd(x, y);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: float64");
        }
    }

    template<typename T>
    [[nodiscard]] constexpr xmm<T> vdiv(const xmm<T> x, const xmm<T> y) noexcept {
        if constexpr (std::is_same_v<T, double>) {
            return _mm_div_pd(x, y);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: float64");
        }
    }

    template<typename T>
    [[nodiscard]] constexpr xmm<T> vsqrt(const xmm<T> x) noexcept {
        if constexpr (std::is_same_v<T, double>) {
            return _mm_sqrt_pd(x);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: float64");
        }
    }

    template<typename T>
    [[nodiscard]] constexpr xmm<T> vswap(const xmm<T> x) noexcept {
        if constexpr (std::is_same_v<T, double>) {
            return _mm_shuffle_pd(x, x, 1);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: float64");
        }
    }

    template<typename T1, typename T2>
    concept equal_greater_size = sizeof(T1) >= sizeof(T2);

    // Converts 2 packed int32 values into two floating point values.
    template<typename To>
        requires std::floating_point<To>&& equal_greater_size<To, int32_t>
    [[nodiscard]] constexpr xmm<To> vcvtpi32(const std::byte* addr) noexcept {
        if constexpr (std::is_same_v<To, double>) {
            const __m128i x{ _mm_loadl_epi64(reinterpret_cast<const __m128i*>(addr)) };
            return _mm_cvtepi32_pd(x);
        } else {
            static_assert(!sizeof(To), "the only supported type for this operation is: int32 -> float64");
        }
    }

    template<typename T>
    [[nodiscard]] constexpr xmm<T> vbcasti64(const uint64_t x) noexcept {
        if constexpr (std::is_same_v<T, double>) {
            const __m128i v{ _mm_set1_epi64x(x) };
            return _mm_castsi128_pd(v);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: int64 -> float64");
        }
    }
}

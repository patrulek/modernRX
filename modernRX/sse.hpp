#pragma once

#include <array>
#include <bit>
#include <intrin.h>
#include <cfenv>

namespace modernRX::intrinsics::sse {
    inline constexpr uint32_t rx_mxcsr_default{ 0x9FC0 }; //Flush to zero, denormals are zero, default rounding mode, all exceptions disabled

    class FloatEnvironment {
    public:
        explicit FloatEnvironment(const uint32_t csr = rx_mxcsr_default) {
            fegetenv(&fenv);
            _mm_setcsr(csr); // reset float state
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

    inline void setFloatRoundingMode(const uint32_t mode, const uint32_t csr = rx_mxcsr_default) {
        _mm_setcsr(csr | (mode << 13));
    }

    template<typename T>
    using xmm alignas(16) = std::array<T, 16 / sizeof(T)>;

    template<typename T>
    xmm<T> vxor(xmm<T> x, xmm<T> y) {
        if constexpr (std::is_same<T, double>::value) {
            return std::bit_cast<xmm<T>>(_mm_xor_pd(std::bit_cast<__m128d>(x), std::bit_cast<__m128d>(y)));
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: float64");
        }
    }

    template<typename T>
    xmm<T> vand(xmm<T> x, xmm<T> y) {
        if constexpr (std::is_same<T, double>::value) {
            return std::bit_cast<xmm<T>>(_mm_and_pd(std::bit_cast<__m128d>(x), std::bit_cast<__m128d>(y)));
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: float64");
        }
    }


    template<typename T>
    xmm<T> vor(xmm<T> x, xmm<T> y) {
        if constexpr (std::is_same<T, double>::value) {
            return std::bit_cast<xmm<T>>(_mm_or_pd(std::bit_cast<__m128d>(x), std::bit_cast<__m128d>(y)));
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: float64");
        }
    }

    template<typename T>
    void store(uintptr_t addr, xmm<T> x) {
        if constexpr (std::is_same<T, double>::value) {
            return _mm_store_pd(addr, std::bit_cast<__m128d>(x));
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: float64");
        }
    }

    template<typename T>
    xmm<T> add(xmm<T> x, xmm<T> y) {
        if constexpr (std::is_same<T, double>::value) {
            return std::bit_cast<xmm<T>>(_mm_add_pd(std::bit_cast<__m128d>(x), std::bit_cast<__m128d>(y)));
        } 
        else {
            static_assert(!sizeof(T), "the only supported type for this operation is: float64");
        }
    }

    template<typename T>
    xmm<T> sub(xmm<T> x, xmm<T> y) {
        if constexpr (std::is_same<T, double>::value) {
            return std::bit_cast<xmm<T>>(_mm_sub_pd(std::bit_cast<__m128d>(x), std::bit_cast<__m128d>(y)));
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: float64");
        }
    }

    template<typename T>
    xmm<T> mul(xmm<T> x, xmm<T> y) {
        if constexpr (std::is_same<T, double>::value) {
            return std::bit_cast<xmm<T>>(_mm_mul_pd(std::bit_cast<__m128d>(x), std::bit_cast<__m128d>(y)));
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: float64");
        }
    }

    template<typename T>
    xmm<T> div(xmm<T> x, xmm<T> y) {
        if constexpr (std::is_same<T, double>::value) {
            return std::bit_cast<xmm<T>>(_mm_div_pd(std::bit_cast<__m128d>(x), std::bit_cast<__m128d>(y)));
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: float64");
        }
    }

    template<typename T>
    xmm<T> sqrt(xmm<T> x) {
        if constexpr (std::is_same<T, double>::value) {
            return std::bit_cast<xmm<T>>(_mm_sqrt_pd(std::bit_cast<__m128d>(x)));
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: float64");
        }
    }

    template<typename T>
    xmm<T> swap(xmm<T> x) {
        if constexpr (std::is_same<T, double>::value) {
            return std::bit_cast<xmm<T>>(_mm_shuffle_pd(std::bit_cast<__m128d>(x), std::bit_cast<__m128d>(x), 1));
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: float64");
        }
    }


    template<typename T1, typename T2>
    concept equal_greater_size = sizeof(T1) >= sizeof(T2);
    
    // Converts 2 packed int32 values into two floating point values.
    template<typename To>
    requires std::floating_point<To> && equal_greater_size<To, int32_t>
    xmm<To> cvtpi32(const std::byte* addr) {
        if constexpr (std::is_same_v<To, double>) {
            __m128i x{ _mm_loadl_epi64(reinterpret_cast<const __m128i*>(addr)) };
            return std::bit_cast<xmm<To>>(_mm_cvtepi32_pd(x));
        }
        else {
            static_assert(!sizeof(To), "the only supported type for this operation is: int32 -> float64");
        }
    }


    template<typename T>
    xmm<T> bcasti64(uint64_t x) {
        __m128i v{ _mm_set1_epi64x(x) };

        if constexpr (std::is_same_v<T, double>) {
            return std::bit_cast<xmm<T>>(_mm_castsi128_pd(v));
        }
        else {
            static_assert(!sizeof(T), "the only supported type for this operation is: int64 -> float64");
        }
    }
}

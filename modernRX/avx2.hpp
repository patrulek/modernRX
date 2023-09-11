#pragma once

#include <array>
#include <intrin.h>
#include <type_traits>

#include "cast.hpp"

namespace modernRX::intrinsics::avx2 {
    template<typename T>
    struct ymm_wrapper {
        using type = std::conditional_t<std::is_integral_v<T>, __m256i,
            std::conditional_t<std::is_same_v<T, double>, __m256d,
            std::conditional_t<std::is_same_v<T, float>, __m256, void>>>;

        static_assert(!std::is_same_v<type, void>, "type can be one of integral types, double or float");
    };

    template<typename T>
    using ymm = ymm_wrapper<T>::type;

    template<typename T, typename... Args>
    [[nodiscard]] constexpr ymm<T> vset(Args... args) noexcept {

        if constexpr (std::is_same_v<T, uint64_t>) {
            if constexpr (sizeof...(Args) == 1) {
                return _mm256_set1_epi64x(args...);
            } else {
                return _mm256_set_epi64x(args...);
            }
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            return _mm256_set_epi32(args...);
        } else {
            static_assert(!sizeof(T), "the only supported types for this operation is uint64");
        }
    }

    template<typename T>
    [[nodiscard]] constexpr ymm<T> vadd(const ymm<T> x, const ymm<T> y) noexcept {
        if constexpr (std::is_same_v<T, uint64_t>) {
            return _mm256_add_epi64(x, y);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: uint64");
        }
    }

    template<typename T>
    [[nodiscard]] constexpr ymm<T> vshift(const ymm<T> x, const int shift) noexcept {
        if constexpr (std::is_same_v<T, uint64_t>) {
            if (shift < 0) {
                return _mm256_slli_epi64(x, -shift);
            } else {
                return _mm256_srli_epi64(x, shift);
            }
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: uint64");
        }
    }

    template<typename T>
    [[nodiscard]] constexpr ymm<T> vmul(const ymm<T> x, const ymm<T> y) noexcept {
        if constexpr (std::is_same_v<T, uint64_t>) {
            return _mm256_mul_epu32(x, y);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: uint64");
        }
    }

    template<typename T>
    [[nodiscard]] constexpr ymm<T> vsub(const ymm<T> x, const ymm<T> y) noexcept {
        if constexpr (std::is_same_v<T, uint64_t>) {
            return _mm256_sub_epi64(x, y);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: uint64");
        }
    }

    template<typename T>
    [[nodiscard]] constexpr ymm<T> vmul64(const ymm<T> x, const ymm<T> y) noexcept {
        if constexpr (std::is_same_v<T, uint64_t>) {
            const auto a1{ _mm256_mul_epu32(x, y) };    // (x & 0xffffffff) * (y & 0xffffffff)
            const auto a2{ _mm256_srli_epi64(x, 32) };  // (x >> 32)
            const auto a3{ _mm256_mul_epu32(a2, y) };   // (x >> 32) * (y & 0xffffffff)
            const auto a4{ _mm256_srli_epi64(y, 32) };  // (y >> 32)
            const auto a5{ _mm256_mul_epu32(x, a4) };   // (x & 0xffffffff) * (y >> 32)
            auto high{ _mm256_add_epi64(a3, a5) };      // (x >> 32) * (y & 0xffffffff) + (x & 0xffffffff) * (y >> 32)
            high = _mm256_slli_epi64(high, 32);         // ((x >> 32) * (y & 0xffffffff) + (x & 0xffffffff) * (y >> 32)) << 32
            return _mm256_add_epi64(a1, high);          // (x & 0xffffffff) * (y & 0xffffffff) + ((x >> 32) * (y & 0xffffffff) + (x & 0xffffffff) * (y >> 32)) << 32
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: uint64");
        }
    }

    // https://stackoverflow.com/a/28827013
    template<typename T>
    [[nodiscard]] constexpr ymm<T> vmulhi64(const ymm<T> x, const ymm<T> y) noexcept {
        if constexpr (std::is_same_v<T, uint64_t>) {
            ymm<T> lomask = _mm256_set1_epi64x(0xffffffff);

            ymm<T> xh = _mm256_shuffle_epi32(x, 0xB1);  // x0l, x0h, x1l, x1h
            ymm<T> yh = _mm256_shuffle_epi32(y, 0xB1);  // y0l, y0h, y1l, y1h

            ymm<T> w0 = _mm256_mul_epu32(x, y);         // x0l*y0l, x1l*y1l
            ymm<T> w1 = _mm256_mul_epu32(x, yh);        // x0l*y0h, x1l*y1h
            ymm<T> w2 = _mm256_mul_epu32(xh, y);        // x0h*y0l, x1h*y0l
            ymm<T> w3 = _mm256_mul_epu32(xh, yh);       // x0h*y0h, x1h*y1h

            ymm<T> w0l = _mm256_and_si256(w0, lomask);
            ymm<T> w0h = _mm256_srli_epi64(w0, 32);

            ymm<T> s1 = _mm256_add_epi64(w1, w0h);
            ymm<T> s1l = _mm256_and_si256(s1, lomask);
            ymm<T> s1h = _mm256_srli_epi64(s1, 32);

            ymm<T> s2 = _mm256_add_epi64(w2, s1l);
            ymm<T> s2l = _mm256_slli_epi64(s2, 32);        
            ymm<T> s2h = _mm256_srli_epi64(s2, 32);

            ymm<T> hi1 = _mm256_add_epi64(w3, s1h);
            return _mm256_add_epi64(hi1, s2h);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: uint64");
        }
    }

    // https://stackoverflow.com/a/28827013
    template<typename T>
    [[nodiscard]] constexpr ymm<T> vsmulhi64(const ymm<T> x, const ymm<T> y) noexcept {
        if constexpr (std::is_same_v<T, uint64_t>) {
            auto hi{ vmulhi64<T>(x, y) };

            //hi -= ((x<0) ? y : 0)  + ((y<0) ? x : 0);
            ymm<T> xs = _mm256_cmpgt_epi64(_mm256_setzero_si256(), x);
            ymm<T> ys = _mm256_cmpgt_epi64(_mm256_setzero_si256(), y);
            ymm<T> t1 = _mm256_and_si256(y, xs);
            ymm<T> t2 = _mm256_and_si256(x, ys);

            hi = _mm256_sub_epi64(hi, t1);
            return _mm256_sub_epi64(hi, t2);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: uint64");
        }
    }

    constexpr void vtranspose8x4pi64(ymm<uint64_t>(&mx)[8]) noexcept {
        for (int i = 0; i < 8; i += 2) {
            auto tmp1 = mx[i];
            mx[i] = _mm256_permute2x128_si256(mx[i + 1], mx[i], 0x02);
            mx[i + 1] = _mm256_permute2x128_si256(mx[i + 1], tmp1, 0x13);
        }

        for (int i = 0; i < 8; i+=4) {
            auto tmp1 = mx[i];
            mx[i] = _mm256_unpacklo_epi64(mx[i], mx[i+2]);
            mx[i] = _mm256_permute4x64_epi64(mx[i], 0b11'01'10'00);
            mx[i+2] = _mm256_unpackhi_epi64(tmp1, mx[i+2]);
            mx[i+2] = _mm256_permute4x64_epi64(mx[i+2], 0b11'01'10'00);

            tmp1 = mx[i + 1];
            mx[i+1] = _mm256_unpacklo_epi64(mx[i+1], mx[i+3]);
            mx[i+1] = _mm256_permute4x64_epi64(mx[i+1], 0b11'01'10'00);
            mx[i+3] = _mm256_unpackhi_epi64(tmp1, mx[i+3]);
            mx[i+3] = _mm256_permute4x64_epi64(mx[i+3], 0b11'01'10'00);
        }

        auto tmp1 = mx[1];
        mx[1] = mx[4];
        mx[4] = tmp1;

        tmp1 = mx[3];
        mx[3] = mx[6];
        mx[6] = tmp1;
    }

    template<typename T>
    [[nodiscard]] constexpr ymm<T> vxor(const ymm<T> x, const ymm<T> y) noexcept {
        if constexpr (std::is_same_v<T, uint64_t>) {
            return _mm256_xor_si256(x, y);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: uint64");
        }
    }

    template<typename T>
    [[nodiscard]] constexpr ymm<T> vand(const ymm<T> x, const ymm<T> y) noexcept {
        if constexpr (std::is_same_v<T, uint64_t>) {
            return _mm256_and_si256(x, y);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: uint64");
        }
    }

    template<typename T>
    [[nodiscard]] constexpr ymm<T> vor(const ymm<T> x, const ymm<T> y) noexcept {
        if constexpr (std::is_same_v<T, uint64_t>) {
            return _mm256_or_si256(x, y);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: uint64");
        }
    }

    template<typename T, int imm8>
    [[nodiscard]] constexpr ymm<T> vblendepi32(const ymm<T> x, const ymm<T> y) noexcept {
        if constexpr (std::is_same_v<T, uint64_t>) {
            return _mm256_blend_epi32(x, y, imm8);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: uint64");
        }
    }

    template<typename T>
    [[nodiscard]] constexpr ymm<T> vunpackloepi64(const ymm<T> x, const ymm<T> y) noexcept {
        if constexpr (std::is_same_v<T, uint64_t>) {
            return _mm256_unpacklo_epi64(x, y);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: uint64");
        }
    }

    template<typename T, int imm8>
    [[nodiscard]] constexpr ymm<T> valignrepi8(const ymm<T> x, const ymm<T> y) noexcept {
        if constexpr (std::is_same_v<T, uint64_t>) {
            return _mm256_alignr_epi8(x, y, imm8);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: uint64");
        }
    }

    template<typename T>
    [[nodiscard]] constexpr ymm<T> vunpackhiepi64(const ymm<T> x, const ymm<T> y) noexcept {
        if constexpr (std::is_same_v<T, uint64_t>) {
            return _mm256_unpackhi_epi64(x, y);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: uint64");
        }
    }

    template<typename T>
    [[nodiscard]] constexpr ymm<T> vbcasti128(const void* addr) noexcept {
        if constexpr (std::is_integral_v<T>) {
            return _mm256_broadcastsi128_si256(_mm_loadu_si128(reinterpret_cast<const __m128i *>(addr)));
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation are integral types");
        }
    }

    template<typename T, int imm8>
    [[nodiscard]] constexpr ymm<T> vshuffleepi32(const ymm<T> x) noexcept {
        if constexpr (std::is_integral_v<T>) {
            return _mm256_shuffle_epi32(x, imm8);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation are integral types");
        }
    }

    template<typename T, typename... Args>
    requires (std::is_convertible_v<Args, char> && ...)
    [[nodiscard]] constexpr ymm<T> vsetrepi8(Args... args) noexcept {
        static_assert(sizeof...(Args) == 32, "Invalid number of arguments");

        if constexpr (std::is_integral_v<T>) {
            return _mm256_setr_epi8(static_cast<char>(args)...);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation are integral types");
        }
    }

    template<typename T>
    [[nodiscard]] constexpr ymm<T> vshuffleepi8(const ymm<T> x, const ymm<T> y) noexcept {
        if constexpr (std::is_integral_v<T>) {
            return _mm256_shuffle_epi8(x, y);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation are integral types");
        }
    }

    // Right rotate packed 64-bit integers.
    // shift cannot be greater than 63.
    template<typename T, uint32_t Shift>
    [[nodiscard]] constexpr ymm<T> vrorpi64(const ymm<T> x) noexcept {
        static_assert(Shift > 0 && Shift < 64, "Shift must be in range 1-63");

        if constexpr (std::is_same_v<T, uint64_t>) {
            // value >> shift | value << (64 - shift);
            return vor<T>(_mm256_srli_epi64(x, Shift), _mm256_slli_epi64(x, 64 - Shift));
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: uint64");
        }
    }

    // Right rotate packed 64-bit integers.
    template<typename T>
    [[nodiscard]] constexpr ymm<T> vrorpi64(const ymm<T> x, const int shift) noexcept {
        if constexpr (std::is_same_v<T, uint64_t>) {
            // value >> shift | value << (64 - shift);
            return vor<T>(vshift<T>(x, shift), vshift<T>(x, shift - 64));
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: uint64");
        }
    }


    template<typename T, int imm8>
    [[nodiscard]] constexpr ymm<T> vpermuteepi64(const ymm<T> x) noexcept {
        static_assert(imm8 >= 0 && imm8 <= 255, "imm8 must be in range 0-255");

        if constexpr (std::is_same_v<T, uint64_t>) {
            return _mm256_permute4x64_epi64(x, imm8);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: uint64");
        }
    }

    // Loads integer values from aligned memory location.
    template<typename T>
    [[nodiscard]] constexpr ymm<T> loadsi256(const void* addr) noexcept {
        if constexpr (std::is_integral_v<T>) {
            return _mm256_load_si256(reinterpret_cast<const __m256i*>(addr));
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation are integral types");
        }
    }

    // Stores integer in address.
    template<typename T>
    [[nodiscard]] constexpr void storesi256(const ymm<T> x, void* addr) noexcept {
        if constexpr (std::is_integral_v<T>) {
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(addr), x);
            return;
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation are integral types");
        }
    }
}

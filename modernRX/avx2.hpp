#pragma once

/*
* Wrapper over some AVX2 intrinsics for speeding up some RandomX algorithm parts.
* Used, but not defined by RandomX algorithm.
* Code may be a little bit messy and not fully documented as this will be further extended in unknown direction.
*/

#include <array>

#include "assertume.hpp"
#include "cast.hpp"
#include "intrinsics.hpp"

namespace modernRX::intrinsics::avx2 {
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
    [[nodiscard]] constexpr ymm<T> vmul(const ymm<T> x, const ymm<T> y) noexcept {
        if constexpr (std::is_same_v<T, uint64_t>) {
            return _mm256_mul_epu32(x, y);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: uint64");
        }
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


    template<typename T, int imm8>
    [[nodiscard]] constexpr ymm<T> vpermuteepi64(const ymm<T> x) noexcept {
        static_assert(imm8 >= 0 && imm8 <= 255, "imm8 must be in range 0-255");

        if constexpr (std::is_same_v<T, uint64_t>) {
            return _mm256_permute4x64_epi64(x, imm8);
        } else {
            static_assert(!sizeof(T), "the only supported type for this operation is: uint64");
        }
    }
}

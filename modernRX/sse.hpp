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

#include "intrinsics.hpp"

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

    template<typename T>
    [[nodiscard]] constexpr xmm<T> vload(const void* ptr) noexcept {
        if constexpr (std::is_same_v<T, double>) {
            return _mm_load_pd(reinterpret_cast<const double*>(ptr));
        } else if constexpr (std::is_integral_v<T>) {
            return _mm_load_si128(reinterpret_cast<const __m128i*>(ptr));
        } else {
            static_assert(!sizeof(T), "the only supported types for this operation are: integral and float64");
        }
    }
}

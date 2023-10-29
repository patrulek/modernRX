#pragma once

/*
* Single-threaded, AVX2 supported and RandomX-specialized implementation of Blake2b based on: https://datatracker.ietf.org/doc/html/rfc7693 and https://github.com/tevador/RandomX
* This is used by Argon2d algorithm, Blake2bRandom, Aes4RGenerator and by the RandomX algorithm to calculate final hash.
*/

#include <array>
#include <span>

#include "aliases.hpp"
#include "avx2.hpp"

namespace modernRX::blake2b {
    inline constexpr uint32_t Block_Size{ 128 }; // In bytes. Not fully filled blocks are padded with zeros.

    // Initialization vector.
    inline const alignas(32) intrinsics::avx2::ymm<uint64_t> IV1{ intrinsics::avx2::vset<uint64_t>(0xa54ff53a5f1d36f1, 0x3c6ef372fe94f82b, 0xbb67ae8584caa73b, 0x6a09e667f3bcc908) };
    inline const alignas(32) intrinsics::avx2::ymm<uint64_t> IV2{ intrinsics::avx2::vset<uint64_t>(0x5be0cd19137e2179, 0x1f83d9abfb41bd6b, 0x9b05688c2b3e6c1f, 0x510e527fade682d1) };

    inline constexpr uint32_t Max_Digest_Size{ 64 }; // In bytes. Must be equal to size of initialization vector.
    static_assert(Max_Digest_Size == sizeof(IV1) + sizeof(IV2));

    /* 
    * Uses given input data and stores Blake2b hash in output parameter.
    * RandomX uses this function in several places with fixed output and input sizes, thus some assumptions were made to optimize this function.
    *   - Output's size is simultaneously a digest size and it is always 32 or 64 bytes.
    *   - Input size is always between 1 and 256 bytes.
    *   - Key parameter is never used, thus it was removed.
    * 
    * Input may point to the same buffer as output.
    */
    void hash(std::span<std::byte> output, const_span<std::byte> input) noexcept;

    // The content of this namespace should be internal, but Argon2d implementation relies on these.
    inline namespace internal {
        // Holds current state of blake2b algorithm.
        // RandomX uses blake2b in specific way, thus some assumptions were made to optimize this struct:
        //   - No need for second counter (input is never greater than 2^64 bytes).
        //   - Key parameter is never used, so initializing context was simplified.
        struct alignas(64) Context {
            // Initializes blake2b state.
            [[nodiscard]] explicit Context(const uint32_t digest_size) noexcept;

            std::array<std::byte, Block_Size> block{};                               // Block buffer to compress.
            std::array<intrinsics::avx2::ymm<uint64_t>, 2> state{ IV1, IV2 };        // Chained state that will yield hash.
            uint64_t counter{ 0 };                                                   // Total number of processed bytes.
            size_t digest_size{ 0 };                                                 // Output size.
        };

        // Fills block buffer with input and updates counter.
        // This function is used with some input that are zeros thus template parameter was added to optimize this function:
        //   - Context.block is always zero-initialized, so if input is empty, there is only need to update counter.
        // This function is used with input that is never greater than 128 bytes, thus some assumptions were made to optimize this function:
        //   - compress function is explicitly called where needed.
        template<bool Empty = false>
        void update(Context& ctx, const_span<std::byte> input) noexcept;

        // Compress last block and generates final state that is used to yield a blake2b hash.
        // RandomX uses this function with fixed output size (64 or 1024 only), so those assumptions were made to optimize this function.
        void final(std::span<std::byte> hash, Context& ctx) noexcept;
    }
};

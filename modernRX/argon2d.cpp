#include <bit>
#include <format>

#include "argon2d.hpp"
#include "argon2davx2.hpp"
#include "assertume.hpp"
#include "avx2.hpp"
#include "blake2b.hpp"
#include "cast.hpp"
#include "intrinsics.hpp"

namespace modernRX::argon2d {
    namespace {
        constexpr uint32_t Initial_Hash_Size{ 64 }; 
        constexpr uint32_t Sync_Points{ 4 }; // Number of equally long lane slices. End of slice can be treated as synchronization point.

        // Holds information about block indexes used in mixing function.
        struct BlockContext {
            uint64_t cur_idx;
            uint64_t prev_idx;
            uint64_t ref_idx;
        };

        [[nodiscard]] consteval uint32_t blocksPerLane() noexcept;
        [[nodiscard]] consteval uint32_t blocksPerSlice() noexcept;
        [[nodiscard]] std::array<std::byte, Initial_Hash_Size> initialize(const_span<std::byte> password) noexcept;
        void makeFirstPass(std::span<Block> memory, const_span<std::byte, Initial_Hash_Size> hash) noexcept;
        void makeSecondPass(std::span<Block> memory) noexcept;

        template<bool XorBlocks>
        void mixBlocks(std::span<Block> memory, BlockContext& ctx) noexcept;

        template<bool XorBlocks>
        void calcRefIndex(std::span<Block> memory, BlockContext& ctx, const uint32_t tmp_value) noexcept;
    }

    void fillMemory(std::span<Block> memory, const_span<std::byte> password) noexcept {
        // Some assumptions were made to optimize this function:
        //   - This function is only called with memory that size is Rx_Argon2d_Memory_Blocks.
        //   - This function is only called with password that size is equal to block template.
        ASSERTUME(memory.size() == Rx_Argon2d_Memory_Blocks);
        ASSERTUME(password.size() > 0 && password.size() <= Rx_Block_Template_Size);

        const auto hash{ initialize(password) };
        makeFirstPass(memory, hash);
        makeSecondPass(memory);
    }

    namespace blake2b {
        void hash(std::span<std::byte> output, const_span<std::byte> input) noexcept {
            // Some assumptions were made to optimize this function:
            //   - This function is only called with input that size is 72 bytes.
            //   - This function is only called with output that size is 1024 bytes.
            static_assert(Initial_Hash_Size + 8 + 4 <= modernRX::blake2b::Block_Size, "Below assumptions can be made only if input for initial hash size fit in single blake2b block.");
            ASSERTUME(output.size() == Block_Size);
            ASSERTUME(input.size() == Initial_Hash_Size + 8);
            ASSERTUME(Block_Size > Initial_Hash_Size);

            using namespace modernRX::blake2b;

            constexpr uint32_t digest_size{ Block_Size };
            Context ctx{ Initial_Hash_Size };

            update(ctx, span_cast<const std::byte, sizeof(uint32_t)>(digest_size));
            update(ctx, input);
            final(output.subspan(0, Max_Digest_Size), ctx);

            constexpr uint32_t step{ Max_Digest_Size / 2 };
            constexpr uint32_t max_offset{ digest_size - Max_Digest_Size };

            for (uint32_t offset = step; offset <= max_offset; offset += step) {
                auto out{ std::span<std::byte>{ output.begin() + offset, output.begin() + offset + Max_Digest_Size } };
                const auto in{ std::span<std::byte>{ output.begin() + offset - step, output.begin() + offset - step + Max_Digest_Size} };

                // Use vanilla blake2b to produce next digest.
                modernRX::blake2b::hash(out, in);
            }
        }
    }

    namespace {
        // Returns number of argon2d blocks per single lane.
        consteval uint32_t blocksPerLane() noexcept {
            return Rx_Argon2d_Memory_Blocks / Rx_Argon2d_Parallelism;
        }

        // Returns number of argon2d blocks per single slice.
        consteval uint32_t blocksPerSlice() noexcept {
            return blocksPerLane() / Sync_Points;
        }

        // Calculates initial 64-bytes long Blake2b hash from argon2d parameters according to first step 
        // from https://github.com/P-H-C/phc-winner-argon2/blob/master/argon2-specs.pdf section 3.2
        std::array<std::byte, Initial_Hash_Size> initialize(const_span<std::byte> password) noexcept {
            // Some assumptions were made to optimize this function:
            //   - Rx_Argon2d_Salt is required by RandomX to be at least 8 bytes long.
            //   - This function is only called with password that size is equal to block template.
            ASSERTUME(Rx_Argon2d_Salt.size() >= 8);
            ASSERTUME(password.size() > 0 && password.size() <= Rx_Block_Template_Size);

            using namespace modernRX::blake2b;

            Context ctx{ Initial_Hash_Size };

            // Incrementally update the hash context with all inputs.
            update(ctx, span_cast<const std::byte, sizeof(uint32_t)>(Rx_Argon2d_Parallelism));
            update<Rx_Argon2d_Tag_Length == 0>(ctx, span_cast<const std::byte, sizeof(uint32_t)>(Rx_Argon2d_Tag_Length));
            update(ctx, span_cast<const std::byte, sizeof(uint32_t)>(Rx_Argon2d_Memory_Blocks));
            update(ctx, span_cast<const std::byte, sizeof(uint32_t)>(Rx_Argon2d_Iterations));
            update(ctx, span_cast<const std::byte, sizeof(uint32_t)>(Rx_Argon2d_Version));
            update<Rx_Argon2d_Type == 0>(ctx, span_cast<const std::byte, sizeof(uint32_t)>(Rx_Argon2d_Type));
            update(ctx, span_cast<const std::byte, sizeof(uint32_t)>(password.size()));
            update(ctx, password);
            update(ctx, span_cast<const std::byte, sizeof(uint32_t)>(Rx_Argon2d_Salt.size()));
            update(ctx, Rx_Argon2d_Salt);

            // If theres nothing to hash, only increase counter inside update.
            if constexpr (Rx_Argon2d_Secret.size() == 0) {
                update<true>(ctx, span_cast<const std::byte, sizeof(uint32_t)>(0));
            } else {
                update(ctx, span_cast<const std::byte, sizeof(uint32_t)>(Rx_Argon2d_Secret.size()));
                update(ctx, password);
            }

            // If theres nothing to hash, only increase counter inside update.
            if constexpr (Rx_Argon2d_Data.size() == 0) {
                update<true>(ctx, span_cast<const std::byte, sizeof(uint32_t)>(0));
            } else {
                update(ctx, span_cast<const std::byte, sizeof(uint32_t)>(Rx_Argon2d_Data.size()));
                update(ctx, Rx_Argon2d_Data);
            }

            std::array<std::byte, Initial_Hash_Size> hash{};
            final(hash, ctx);

            return hash;
        }

        /*
        * Performs first iteration of filling memory in two steps:
        * 1. Initializes first three blocks of every memory lane, because they are special cases:
        *    block[0] = Blake2HashLong(hash .. 0 .. lane_index)
        *    block[1] = Blake2HashLong(hash .. 1 .. lane_index)
        *    block[2] = Mix(block[0], block[1])
        * 2. Initializes all other blocks in a lane.
        *    block[n] = Mix(block[n-1], block[rand()%(n-1)])
        * 
        *   where `rand() % (n-1)` is defined by https://github.com/P-H-C/phc-winner-argon2/blob/master/argon2-specs.pdf section 3.3.
        *   In general in this step, the referenced random block may be one of the blocks already computed in this step or step 1.
        *
        * Memory is arranged like this (every slice has equal number of blocks):
        *
        *         || slice[0]                  || slice[1]                  || slice[2]                  || slice[3]                   |
        *         || ------------------------- || ------------------------- || ------------------------- || -------------------------- |
        *         || block[0] | block[1] | ... || .... | .... | .... | .... || .... | .... | .... | .... || ... | ... | ... | block[n] |
        * ========||==========|==========|=====||======|======|======|======||======|======|======|======||=====|=====|=====|==========|
        * lane[0] || aaaaaaaa | bbbbbbbb | ... || .... | .... | .... | .... || .... | .... | .... | .... || ... | ... | ... | cccccccc |
        * 
        * Beginning of (or ending, depending on PoV) slice is a synchronization point in which threads must synchronize their
        * states to be able to reference blocks in other lanes from previous (already processed) slices.
        *
        * Function was simplified with assumption that parallelism is always 1, thus no synchronization is needed because there is always only one thread.
        */
        void makeFirstPass(std::span<Block> memory, const_span<std::byte, Initial_Hash_Size> hash) noexcept {
            // Below assertion is very limiting and removing this will not simply make function work with other values.
            static_assert(Rx_Argon2d_Parallelism == 1, "This simplification requires parallelism to be 1.");
            ASSERTUME(memory.size() == Rx_Argon2d_Memory_Blocks);

            using namespace modernRX::blake2b;

            // Calculate first three blocks in a lane.
            std::array<std::byte, Initial_Hash_Size + 2 * sizeof(uint32_t)> input{};
            std::memcpy(input.data(), hash.data(), Initial_Hash_Size);

            // First block.
            blake2b::hash(std::span<std::byte>{ memory[0] }, input);

            // Second block.
            input[Initial_Hash_Size] = std::byte{ 0x01 };
            blake2b::hash(std::span<std::byte>{ memory[1] }, input);

            // Third block is always mix of first two.
            // Calculate next blocks in a lane.
            for (BlockContext ctx{ 2, 1, 0 }; ctx.cur_idx < blocksPerLane(); ) {
                mixBlocks<false>(memory, ctx);
            }
        }

        /*
        * Performs all, except first, iterations of filling memory in two steps:
        * 1. Calculate first block in lane.
        * 2. Calculate all other blocks in lane.
        *
        * The main difference between this and first iteration is that referenced block will be the one of
        * the block from previous iteration or the block from current iteration. It all depends if referenced block
        * lies in rolling window of size equal to three full slices.
        * 
        * This function was simplified with assumption that parallelism is always 1.
        *
        * For more details check https://github.com/P-H-C/phc-winner-argon2/blob/master/argon2-specs.pdf section 3.2 and 3.3.
        */
        void makeSecondPass(std::span<Block> memory) noexcept {
            // Below assertion is very limiting and removing this will not simply make function work with other values.
            static_assert(Rx_Argon2d_Parallelism == 1, "This simplification requires parallelism to be 1.");
            ASSERTUME(memory.size() == Rx_Argon2d_Memory_Blocks);

            constexpr auto prev_idx{ blocksPerLane() - 1 };
            constexpr auto ref_length{ blocksPerLane() - blocksPerSlice() - 1 };
            constexpr auto shift_index{ blocksPerSlice() };

            for (uint32_t round = 1; round < Rx_Argon2d_Iterations; ++round) {
                // Calculate first block.
                const auto J1{ bytes_cast<uint64_t>(memory[prev_idx]) & 0x00000000ffffffff };
                const auto x{ (J1 * J1) >> 32 };
                const auto y{ (ref_length * x) >> 32 };
                const auto z{ ref_length - 1 - y };
                const auto ref_index{ shift_index + z }; // No need to modulo by blocks_per_lane because it will always be less than blocks_per_lane.

                BlockContext ctx{ 0, prev_idx, ref_index };
                mixBlocks<true>(memory, ctx);

                // Calculate all next blocks in a lane.
                for (; ctx.cur_idx < blocksPerLane(); ) {
                    mixBlocks<true>(memory, ctx);
                }
            }
        }

        // Calculates reference index needed for mixing function and updates BlockContext for next iteration.
        // This function is called inside ROUND_V2WithPrefetch macro to prefetch referenced block as soon as possible and reduce stalls because of cache misses.
        template<bool XorBlocks>
        void calcRefIndex(std::span<Block> memory, BlockContext& ctx, const uint32_t tmp_value) noexcept {
            static_assert(Rx_Argon2d_Parallelism == 1, "This simplification requires parallelism to be 1.");

            ++ctx.cur_idx;
            auto J1{ static_cast<uint64_t>(bytes_cast<uint32_t>(memory[ctx.prev_idx]) ^ bytes_cast<uint32_t>(memory[ctx.ref_idx]) ^ tmp_value) };
            
            if constexpr (XorBlocks) {
                // Map current block index in lane to current slice and index within current slice.
                const auto slice_idx{ ctx.cur_idx % blocksPerSlice() };
                const auto slice{ ctx.cur_idx / blocksPerSlice() };
                
                const auto ref_length{ blocksPerLane() - blocksPerSlice() + slice_idx - 1 }; // Up to (sync_points - 1) * blocks_per_slice plus number of blocks processed in current slice - 1 can be picked up.
                const auto shift_index{ blocksPerSlice() * ((slice + 1) % Sync_Points) }; // Start at beginning of (slice - 3; roll over if overflows).

                // Calculate reference index.
                J1 ^= bytes_cast<uint32_t>(memory[ctx.cur_idx - 1]);
                const auto x{ (J1 * J1) >> 32 };
                const auto y{ (ref_length * x) >> 32 };
                const auto z{ ref_length - 1 - y };

                ctx.ref_idx = (shift_index + z) % blocksPerLane(); // This may overflow blocksPerLane() so have to be rolled over (% blocksPerLane()).
            } else {
                const auto ref_length{ ctx.prev_idx + 1 }; // For single lane and first iteration it will always be up to previous block.

                // Calculate reference index.
                const auto x{ (J1 * J1) >> 32 };
                const auto y{ (ref_length * x) >> 32 };

                ctx.ref_idx = ref_length - 1 - y;
            }

            ctx.prev_idx = (ctx.prev_idx + 1) % blocksPerLane();
        }


        // Calculates current block based on previous and referenced random block.
        // If its first iteration XorBlocks should be false, otherwise should be true and will perform xor operation with overwritten block.
        // The mixing function refers to https://github.com/P-H-C/phc-winner-argon2/blob/master/argon2-specs.pdf section 3.4.
        // Enhanced by AVX2 intrinsics.
        template<bool XorBlocks>
        void mixBlocks(std::span<Block> memory, BlockContext& ctx) noexcept {
            using namespace intrinsics;

            constexpr uint32_t YMM_Per_Block{ Block_Size / sizeof(ymm<uint64_t>) };

            // For some reason its faster to use C-array than std::array (at least for tmp_block_ymm).
            // It's already reported as a compiler bug: https://developercommunity.visualstudio.com/t/Performance-degradation-when-using-std::/10460894.
            alignas(64) ymm<uint64_t> tmp_block_ymm[YMM_Per_Block];
            ymm<uint64_t> (&cur_block_ymm)[YMM_Per_Block]{ reinterpret_cast<ymm<uint64_t>(&)[YMM_Per_Block]>(memory[ctx.cur_idx]) };
            const ymm<uint64_t> (&prev_block_ymm)[YMM_Per_Block] { reinterpret_cast<const ymm<uint64_t>(&)[YMM_Per_Block]>(memory[ctx.prev_idx]) };
            const ymm<uint64_t> (&ref_block_ymm)[YMM_Per_Block] { reinterpret_cast<const ymm<uint64_t>(&)[YMM_Per_Block]>(memory[ctx.ref_idx]) };

            // Initialize new block with previous and referenced ones.
            for (uint32_t i = 0; i < YMM_Per_Block; ++i) {
                tmp_block_ymm[i] = avx2::vxor<uint64_t>(prev_block_ymm[i], ref_block_ymm[i]);
            }

            // Prepare rotation constant. Taken from: https://github.com/jedisct1/libsodium/blob/1.0.16/src/libsodium/crypto_generichash/blake2b/ref/blake2b-compress-avx2.h
            const auto avx_rot24{ avx2::vsetrepi8<uint64_t>(3, 4, 5, 6, 7, 0, 1, 2, 11, 12, 13, 14, 15, 8, 9, 10, 3, 4, 5, 6, 7, 0, 1, 2, 11, 12, 13, 14, 15, 8, 9, 10) };
            const auto avx_rot16{ avx2::vsetrepi8<uint64_t>(2, 3, 4, 5, 6, 7, 0, 1, 10, 11, 12, 13, 14, 15, 8, 9, 2, 3, 4, 5, 6, 7, 0, 1, 10, 11, 12, 13, 14, 15, 8, 9) };

            // Used in macros.
            ymm<uint64_t> ml, ml2;

            // Both rounding loops are crucial for performance, otherwise function would expand too much and compiler would not be willing to inline it.

            // Apply blake2b "rowwise", ie. elements (0,1,...,31), then (32,33,...,63) ... finally (96,97,...,127).
            for (uint32_t i = 0; i < 4; ++i) {
                ROUND_V1(tmp_block_ymm[8 * i], tmp_block_ymm[8 * i + 4], tmp_block_ymm[8 * i + 1], tmp_block_ymm[8 * i + 5],
                    tmp_block_ymm[8 * i + 2], tmp_block_ymm[8 * i + 6], tmp_block_ymm[8 * i + 3], tmp_block_ymm[8 * i + 7], avx_rot24, avx_rot16);
            }

            // Apply first iteration of blake2b "columnwise".
            // This macro version calls calcRefIndex to calculate reference index early for prefetching to reduce stalls.
            ROUND_V2WithPrefetch(tmp_block_ymm[0], tmp_block_ymm[4], tmp_block_ymm[8], tmp_block_ymm[12],
                tmp_block_ymm[16], tmp_block_ymm[20], tmp_block_ymm[24], tmp_block_ymm[28], avx_rot24, avx_rot16);


            // Apply all other blake2b "columnwise" rounds, ie. elements (0,1,2,3,16,17,18,19,...,112,113,114,115), then (4,5,6,7,20,21,22,23,...,116,117,118,119) 
            // ... finally (12,13,14,15,28,29,30,31,...,124,125,126,127).
            for (uint32_t i = 1; i < 4; ++i) {
                ROUND_V2(tmp_block_ymm[i], tmp_block_ymm[4 + i], tmp_block_ymm[8 + i], tmp_block_ymm[12 + i],
                    tmp_block_ymm[16 + i], tmp_block_ymm[20 + i], tmp_block_ymm[24 + i], tmp_block_ymm[28 + i], avx_rot24, avx_rot16);
            }

            // Finalize new block.
            for (uint32_t i = 0; i < YMM_Per_Block; ++i) {
                if constexpr (XorBlocks) {
                    cur_block_ymm[i] = avx2::vxor<uint64_t>(avx2::vxor<uint64_t>(avx2::vxor<uint64_t>(prev_block_ymm[i], tmp_block_ymm[i]), cur_block_ymm[i]), ref_block_ymm[i]);
                } else {
                    cur_block_ymm[i] = avx2::vxor<uint64_t>(avx2::vxor<uint64_t>(prev_block_ymm[i], tmp_block_ymm[i]), ref_block_ymm[i]);
                }
            }
        }
    }
}

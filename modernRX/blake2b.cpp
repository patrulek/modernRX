#include <bit>
#include <format>

#include "assertume.hpp"
#include "blake2b.hpp"
#include "blake2bavx2.hpp"
#include "cast.hpp"
#include "randomxparams.hpp"

namespace modernRX::blake2b {
	namespace {
		template<bool Last>
		void compress(Context& ctx) noexcept;
	}

	void hash(std::span<std::byte> output, const_span<std::byte> input) noexcept {
		// Some assumptions were made to optimize this function:
		//   - This function is only called with input that size is between 1 and 256.
		//   - This function is only called with output that size is equal to 32 or 64.
		static_assert(Rx_Block_Template_Size <= 256, "Below assumption can be made only if blockchain's block template size is lesser than VM's RegisterFile, which is 256 bytes long");
		ASSERTUME(input.size() > 0 && input.size() <= 256);
		ASSERTUME(output.size() == Max_Digest_Size / 2 || output.size() == Max_Digest_Size);

		const uint32_t digest_size{ static_cast<uint32_t>(output.size()) };

		// Initialize context.
		Context ctx{ digest_size };

		// Special case when input size is lesser-equal than block size.
		if (input.size() <= Block_Size) {
			update(ctx, input);
			final(output, ctx);
			return;
        }
		
		// With above assumption its known that no more than two blocks will be compressed, so we can unroll loop.
		update(ctx, input.subspan(0, Block_Size));
		compress<false>(ctx);
		update(ctx, input.subspan(Block_Size));
		final(output, ctx);
	}

	inline namespace internal {
		Context::Context(const uint32_t digest_size) noexcept
			: digest_size{ digest_size } {
			state[0] ^= 0x01010000 ^ digest_size;
		}

		template void update<true>(Context&, const_span<std::byte>) noexcept;
		template void update<false>(Context&, const_span<std::byte>) noexcept;

		template<bool Empty>
		void update(Context& ctx, const_span<std::byte> input) noexcept {
			static_assert(Rx_Block_Template_Size + sizeof(Rx_Argon2d_Salt) + 40 <= Block_Size, "Below assumption can be made only if blockchain's block template size + argon2d salt + 40 (size of argon2d params) is lesser than blake2b's single block size");
			// This assumption can be made because it is never called with empty input and always with one that is lesser-equal than block size.
			ASSERTUME(input.size() > 0 && input.size() <= Block_Size);

			if constexpr (!Empty) {
				std::memcpy(ctx.block.data() + (ctx.counter % Block_Size), input.data(), input.size());
			}
			ctx.counter += input.size();
		}

		void final(std::span<std::byte> hash, Context& ctx) noexcept {
			// This assumption can be made because this function is only called by argon2d with 64 or 1024 bytes long hash buffer.
			ASSERTUME(hash.size() == 64 || hash.size() == 1024);

			compress<true>(ctx);

			// Copy state into output buffer.
			std::memcpy(hash.data(), ctx.state.data(), hash.size());
		}
	}

	namespace {
		// Compress does all the magic with compressing block buffer. Works differently for last and non-last block buffer.
		// Optimized with AVX2 intrinsics.
		template<bool Last>
		void compress(Context& ctx) noexcept {
			using namespace intrinsics;

			// Prepare block for permutations.
			const_array<avx2::ymm<uint64_t>, 8> msg{
				avx2::vbcasti128<uint64_t>(ctx.block.data()),
				avx2::vbcasti128<uint64_t>(ctx.block.data() + 16),
				avx2::vbcasti128<uint64_t>(ctx.block.data() + 32),
				avx2::vbcasti128<uint64_t>(ctx.block.data() + 48),
				avx2::vbcasti128<uint64_t>(ctx.block.data() + 64),
				avx2::vbcasti128<uint64_t>(ctx.block.data() + 80),
				avx2::vbcasti128<uint64_t>(ctx.block.data() + 96),
				avx2::vbcasti128<uint64_t>(ctx.block.data() + 112)
			};

			// Initialize working vector.
			auto v1{ avx2::loadsi256<uint64_t>(ctx.state.data()) }; // {h[0], h[1], h[2], h[3]}
			auto v2{ avx2::loadsi256<uint64_t>(ctx.state.data() + 4) }; // {h[4], h[5], h[6], h[7]}
			auto v3{ avx2::loadsi256<uint64_t>(IV.data()) }; // {IV[0], IV[1], IV[2], IV[3]}
			auto v4{ avx2::loadsi256<uint64_t>(IV.data() + 4) }; // {IV[4] ^ ctx.count, IV[5], IV[6] ^ oneof(0xffffffff, 0), h[7]}
			v4 = avx2::vxor<uint64_t>(v4, avx2::vset<uint64_t>(0, (Last ? std::numeric_limits<uint64_t>::max() : 0ULL), 0, ctx.counter));

			// Prepare rotation constants. Taken from: https://github.com/jedisct1/libsodium/blob/1.0.16/src/libsodium/crypto_generichash/blake2b/ref/blake2b-compress-avx2.h
			const auto rot24{ avx2::vsetrepi8<uint64_t>(3, 4, 5, 6, 7, 0, 1, 2, 11, 12, 13, 14, 15, 8, 9, 10, 3, 4, 5, 6, 7, 0, 1, 2, 11, 12, 13, 14, 15, 8, 9, 10) };
			const auto rot16{ avx2::vsetrepi8<uint64_t>(2, 3, 4, 5, 6, 7, 0, 1, 10, 11, 12, 13, 14, 15, 8, 9, 2, 3, 4, 5, 6, 7, 0, 1, 10, 11, 12, 13, 14, 15, 8, 9) };

			// Make cryptographic message mixing (12 rounds)
			ROUND(0, msg, v1, v2, v3, v4, rot24, rot16);
			ROUND(1, msg, v1, v2, v3, v4, rot24, rot16);
			ROUND(2, msg, v1, v2, v3, v4, rot24, rot16);
			ROUND(3, msg, v1, v2, v3, v4, rot24, rot16);
			ROUND(4, msg, v1, v2, v3, v4, rot24, rot16);
			ROUND(5, msg, v1, v2, v3, v4, rot24, rot16);
			ROUND(6, msg, v1, v2, v3, v4, rot24, rot16);
			ROUND(7, msg, v1, v2, v3, v4, rot24, rot16);
			ROUND(8, msg, v1, v2, v3, v4, rot24, rot16);
			ROUND(9, msg, v1, v2, v3, v4, rot24, rot16);

			// Round 10 is the same as round 0, and round 11 is the same as round 1
			ROUND(0, msg, v1, v2, v3, v4, rot24, rot16);
			ROUND(1, msg, v1, v2, v3, v4, rot24, rot16);

			// Finalize compression
			avx2::storesi256<uint64_t>(avx2::vxor<uint64_t>(avx2::loadsi256<uint64_t>(ctx.state.data()), avx2::vxor<uint64_t>(v1, v3)), ctx.state.data());
			avx2::storesi256<uint64_t>(avx2::vxor<uint64_t>(avx2::loadsi256<uint64_t>(ctx.state.data() + 4), avx2::vxor<uint64_t>(v2, v4)), ctx.state.data() + 4);
		}
	}
};

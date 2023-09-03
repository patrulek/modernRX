#include <bit>
#include <format>

#include "blake2b.hpp"
#include "cast.hpp"

namespace modernRX::blake2b {
	namespace {
		void round(std::span<uint64_t, 16> v, const_span<uint64_t, 16> m, const size_t r) noexcept;
		void gmixing(std::span<uint64_t, 16> v, const uint32_t a, const uint32_t b, const uint32_t c, const uint32_t d, const uint64_t x, const uint64_t y) noexcept;
	}

	void hash(std::span<std::byte> output, const_span<std::byte> input, const_span<std::byte> key) {
		static constexpr uint32_t Max_Key_Size{ 64 };  // In bytes.

		if (input.size() == 0 || key.size() > Max_Key_Size) {
			throw std::format("invalid data (size: {}) or key (size: {})", input.size(), key.size());
		}

		const uint32_t digest_size{ static_cast<uint32_t>(output.size()) };

		if (digest_size == 0 || digest_size > Max_Digest_Size) {
			throw std::format("invalid digest size: {}", digest_size);
		}

		Context ctx{ init(key, digest_size) };
		update(ctx, input);
		final(output, ctx);
	}

	inline namespace internal {
		Context init(const_span<std::byte> key, const uint32_t digest_size) noexcept {
			Context ctx{
				.digest_size{ digest_size },
			};

			ctx.state[0] ^= 0x01010000 ^ (key.size() << 8) ^ ctx.digest_size;

			if (key.size() == 0) {
				return ctx;
			}

			update(ctx, key);
			compress(ctx, false);
			ctx.block_idx = 0;

			return ctx;
		}

		void update(Context& ctx, const_span<std::byte> input) noexcept {
			for (const auto byte : input) {
				if (ctx.block_idx == Block_Size) {            
					ctx.counters[0] += ctx.block_idx;        

					// This code is not needed, because implementation do not support inputs bigger than 2^64 anyway.
					if (ctx.counters[0] < Block_Size) { // Its true when variable overflows.
						++ctx.counters[1];
					}

					compress(ctx, false);
					ctx.block_idx = 0;
				}

				ctx.block[ctx.block_idx++] = byte;
			}
		}

		void compress(Context& ctx, const bool last) noexcept {
			static constexpr uint32_t Rounds{ 12 };
			std::array<uint64_t, 16> v{};

			for (size_t i = 0; i < 8; i++) {
				v[i] = ctx.state[i]; // Copy current state.
				v[i + 8] = IV[i]; // Copy initialization vector.
			}

			v[12] ^= ctx.counters[0];
			v[13] ^= ctx.counters[1]; // This code is not needed, because implementation do not support inputs bigger than 2^64 anyway.

			if (last) {
				v[14] = ~v[14];
			}

			// Treat each 128-byte block as sixteen 8-byte words
			const auto m{ stdexp::span_cast<uint64_t, 16>(ctx.block) };

			// Make cryptographic message mixing
			for (size_t i = 0; i < Rounds; ++i) {
				round(v, m, i);
			}

			// Mix the upper and lower halves of v
			for (size_t i = 0; i < 8; ++i) {
				ctx.state[i] ^= v[i] ^ v[i + 8];
			}
		}

		void final(std::span<std::byte> hash, Context& ctx) noexcept {
			ctx.counters[0] += ctx.block_idx;

			// This code is not needed, because implementation do not support inputs bigger than 2^64 anyway.
			if (ctx.counters[0] < ctx.block_idx) { // Its true when variable overflows.
				++ctx.counters[1];
			}

			// Pad last block with zeros
			while (ctx.block_idx < Block_Size) {
				ctx.block[ctx.block_idx++] = std::byte{ 0 };
			}

			compress(ctx, true);

			// Copy state into output buffer.
			std::memcpy(hash.data(), ctx.state.data(), ctx.digest_size);
		}
	}

	namespace {
		void round(std::span<uint64_t, 16> v, const_span<uint64_t, 16> m, const size_t r) noexcept {
			gmixing(v, 0, 4, 8, 12, m[Sigma[r][0]], m[Sigma[r][1]]);
			gmixing(v, 1, 5, 9, 13, m[Sigma[r][2]], m[Sigma[r][3]]);
			gmixing(v, 2, 6, 10, 14, m[Sigma[r][4]], m[Sigma[r][5]]);
			gmixing(v, 3, 7, 11, 15, m[Sigma[r][6]], m[Sigma[r][7]]);
			gmixing(v, 0, 5, 10, 15, m[Sigma[r][8]], m[Sigma[r][9]]);
			gmixing(v, 1, 6, 11, 12, m[Sigma[r][10]], m[Sigma[r][11]]);
			gmixing(v, 2, 7, 8, 13, m[Sigma[r][12]], m[Sigma[r][13]]);
			gmixing(v, 3, 4, 9, 14, m[Sigma[r][14]], m[Sigma[r][15]]);
		}

		void gmixing(std::span<uint64_t, 16> v, const uint32_t a, const uint32_t b, const uint32_t c, const uint32_t d, const uint64_t x, const uint64_t y) noexcept {
			v[a] = v[a] + v[b] + x;
			v[d] = std::rotr(v[d] ^ v[a], Rotation_Constants[0]);
			v[c] = v[c] + v[d];
			v[b] = std::rotr(v[b] ^ v[c], Rotation_Constants[1]);
			v[a] = v[a] + v[b] + y;
			v[d] = std::rotr(v[d] ^ v[a], Rotation_Constants[2]);
			v[c] = v[c] + v[d];
			v[b] = std::rotr(v[b] ^ v[c], Rotation_Constants[3]);
		}
	}
};

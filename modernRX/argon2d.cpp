#include <bit>
#include <format>

#include "argon2d.hpp"
#include "blake2b.hpp"
#include "cast.hpp"

namespace modernRX::argon2d {
	namespace {
		constexpr uint32_t Initial_Hash_Size{ ::modernRX::blake2b::Max_Digest_Size }; 
		constexpr uint32_t Sync_Points{ 4 }; // Number of equally long lane slices. End of slice can be treated as synchronization point.

		// Holds algorithm configuration parameters.
		struct Params {
			std::span<std::byte> secret{};							// Serves as key. Size must be in range (0 .. 2^32 - 1).
			std::span<std::byte> data{};							// Associated data. Size must be in range (0 .. 2^32 - 1).
			uint32_t parallelism{ Rx_Argon2d_Parallelism };			// Number of lanes/threads used to fill memory. Must be in range (1 .. 2^24 - 1).
			uint32_t tag_length{ 0 };								// Digest size. RandomX algorithm doesn't need to produce final hash, so this is not used.
			uint32_t memory_blocks{ Rx_Argon2d_Memory_Blocks };		// Number of memory blocks. Defines total amount of memory to fill. Must be in range (8 * parallelism .. 2^32 - 1)
			uint32_t iterations{ Rx_Argon2d_Iterations };			// Number of iterations. Must be in range (1 .. 2^32 - 1).
			uint32_t type{ 0 };										// Enumeration for Argon2 algorithm type. For Argon2d this must be 0.
			uint32_t version{ 0x13 };								// Version of Argon2d impementation. Must be 0x13.
		};

		[[nodiscard]] std::array<std::byte, Initial_Hash_Size> initialize(const_span<std::byte> password, const_span<std::byte> salt, const Params& params) noexcept;
		void makeFirstPass(Memory& memory, const_span<std::byte, 64> hash, const Params& params);
		void makeSecondPass(Memory& memory, const Params& params);
		void mixBlocks(Block& cur_block, const Block& prev_block, const Block& ref_block, const bool xor_blocks);
	}

	void fillMemory(Memory& memory, const_span<std::byte> password, const_span<std::byte> salt) {
		const Params params{};

		if (memory.size() != params.memory_blocks) {
			throw std::format("mismatched configuration; memory.size(): {}, params.memory_blocks: {}", memory.size(), params.memory_blocks);
		}

		if (salt.size() < 8) {
			throw std::format("invalid salt size: {}", salt.size());
		}

		const auto hash{ initialize(password, salt, params) };
		makeFirstPass(memory, hash, params);
		makeSecondPass(memory, params);
	}

	namespace blake2b {
		void gmixing(std::span<uint64_t, 16> v, const uint32_t a, const uint32_t block, const uint32_t block_idx, const uint32_t d) noexcept {
			static constexpr uint64_t mask32 = 0xffffffff;

			v[a] = v[a] + v[block] + 2 * (v[a] & mask32) * (v[block] & mask32);
			v[d] = std::rotr(v[d] ^ v[a], ::modernRX::blake2b::Rotation_Constants[0]);
			v[block_idx] = v[block_idx] + v[d] + 2 * (v[block_idx] & mask32) * (v[d] & mask32);
			v[block] = std::rotr(v[block] ^ v[block_idx], ::modernRX::blake2b::Rotation_Constants[1]);
			v[a] = v[a] + v[block] + 2 * (v[a] & mask32) * (v[block] & mask32);
			v[d] = std::rotr(v[d] ^ v[a], ::modernRX::blake2b::Rotation_Constants[2]);
			v[block_idx] = v[block_idx] + v[d] + 2 * (v[block_idx] & mask32) * (v[d] & mask32);
			v[block] = std::rotr(v[block] ^ v[block_idx], ::modernRX::blake2b::Rotation_Constants[3]);
		}

		void round(std::span<uint64_t, 16> v) noexcept {
			gmixing(v, 0, 4, 8, 12);
			gmixing(v, 1, 5, 9, 13);
			gmixing(v, 2, 6, 10, 14);
			gmixing(v, 3, 7, 11, 15);
			gmixing(v, 0, 5, 10, 15);
			gmixing(v, 1, 6, 11, 12);
			gmixing(v, 2, 7, 8, 13);
			gmixing(v, 3, 4, 9, 14);
		}

		void hash(std::span<std::byte> output, const_span<std::byte> input) {
			using namespace modernRX::blake2b;

			if (input.size() == 0 || input.size() >= std::numeric_limits<uint32_t>::max()) {
				throw std::format("invalid input size: {}", input.size());
			}

			if (output.size() == 0) {
				throw std::format("invalid digest size: {}", output.size());
			}

			const uint32_t digest_size{ static_cast<uint32_t>(output.size()) };
			const uint32_t initial_hash_size{ std::min(digest_size, Max_Digest_Size) };
			Context ctx{ init(std::span<std::byte>{}, initial_hash_size) };

			update(ctx, span_cast<const std::byte, sizeof(uint32_t)>(digest_size));
			update(ctx, input);
			final(output, ctx);

			// If the digest size is in range of vanilla blake2b, it's done.
			if (digest_size == initial_hash_size) {
				return;
			}

			constexpr uint32_t step{ Max_Digest_Size / 2 };
			uint32_t to_produce{ digest_size - step };
			uint32_t offset{ step };

			while (to_produce > Max_Digest_Size) {
				auto out{ std::span<std::byte>{ output.begin() + offset, output.begin() + offset + Max_Digest_Size } };
				const auto in{ std::span<std::byte>{ output.begin() + offset - step, output.begin() + offset - step + Max_Digest_Size} };

				// Use vanilla blake2b to produce next digest.
				modernRX::blake2b::hash(out, in, std::span<std::byte>{});

				offset += step;
				to_produce -= step;
			}

			auto out{ std::span<std::byte>{ output.begin() + offset, output.begin() + offset + to_produce} };
			const auto in{ std::span<std::byte>{ output.begin() + offset - step, output.begin() + offset - step + to_produce} };
			modernRX::blake2b::hash(out, in, std::span<std::byte>{});
		}
	}

	namespace {
		// Calculates 64-bytes long Blake2b hash for a given input according to first step 
		// from https://github.com/P-H-C/phc-winner-argon2/blob/master/argon2-specs.pdf section 3.2
		std::array<std::byte, Initial_Hash_Size> initialize(const_span<std::byte> password, const_span<std::byte> salt, const Params& params) noexcept {
			using namespace modernRX::blake2b;

			Context ctx{ init(std::span<std::byte>{}, Initial_Hash_Size) };

			// Incrementally update the hash context with all inputs.
			update(ctx, span_cast<const std::byte, sizeof(uint32_t)>(params.parallelism));
			update(ctx, span_cast<const std::byte, sizeof(uint32_t)>(params.tag_length));
			update(ctx, span_cast<const std::byte, sizeof(uint32_t)>(params.memory_blocks));
			update(ctx, span_cast<const std::byte, sizeof(uint32_t)>(params.iterations));
			update(ctx, span_cast<const std::byte, sizeof(uint32_t)>(params.version));
			update(ctx, span_cast<const std::byte, sizeof(uint32_t)>(params.type));
			update(ctx, span_cast<const std::byte, sizeof(uint32_t)>(password.size()));
			update(ctx, password);
			update(ctx, span_cast<const std::byte, sizeof(uint32_t)>(salt.size()));
			update(ctx, salt);
			update(ctx, span_cast<const std::byte, sizeof(uint32_t)>(params.secret.size()));
			update(ctx, params.secret);
			update(ctx, span_cast<const std::byte, sizeof(uint32_t)>(params.data.size()));
			update(ctx, params.data);

			std::array<std::byte, Initial_Hash_Size> hash{};
			final(hash, ctx);

			return hash;
		}

		/*
		* Performs first iteration of filling memory in three steps:
		* 1. Initializes first three blocks of every memory lane, because they are special cases:
		*	block[0] = Blake2HashLong(hash .. 0 .. lane_index)
		*	block[1] = Blake2HashLong(hash .. 1 .. lane_index)
		*	block[2] = Mix(block[0], block[1])
		* 2. Initializes the other blocks of the first slice of every lane.
		*	block[n] = Mix(block[n-1], block[rand()%(n-1)])
		* 
		*   where `rand() % (n-1)` is defined by https://github.com/P-H-C/phc-winner-argon2/blob/master/argon2-specs.pdf section 3.3.
		*   In general in this step, the referenced random block may be one of the blocks already computed in this step or step 1.
		* 3. Initializes blocks in the next three slices of every lane.
		*	General rule for getting random blocks in this step stays the same, except that referenced blocks
		*   may belong to other lanes, than the currently processing lane, however if referenced block is taken
		*   from another lane it cannot be block from current slice.
		*
		* Memory is arranged like this (every slice has equal number of blocks):
		*
		*         || slice[0]                  || slice[1]                  || slice[2]                  || slice[3]                   |
		*         || block[0] | block[1] | ... || .... | .... | .... | .... || .... | .... | .... | .... || ... | ... | ... | block[n] |
		* ========||==========|==========|=====||======|======|======|======||======|======|======|======||=====|=====|=====|==========|
		* lane[0] || aaaaaaaa | bbbbbbbb | ... || .... | .... | .... | .... || .... | .... | .... | .... || ... | ... | ... | cccccccc |
		* lane[1] || xxxxxxxx | yyyyyyyy | ... || .... | .... | .... | .... || .... | .... | .... | .... || ... | ... | ... | zzzzzzzz |
		* ....... || ........ | ........ | ... || .... | .... | .... | .... || .... | .... | .... | .... || ... | ... | ... | ........ |
		* lane[p] || ........ | ........ | ... || .... | .... | .... | .... || .... | .... | .... | .... || ... | ... | ... | ........ |
		* 
		* Beginning of (or ending, depending on PoV) slice is a synchronization point in which threads must synchronize their
		* states to be able to reference blocks in other lanes from previous (already processed) slices.
		*/
		void makeFirstPass(Memory& memory, const_span<std::byte, Initial_Hash_Size> hash, const Params& params) {
			using namespace modernRX::blake2b;

			const uint32_t blocks_per_lane{ params.memory_blocks / params.parallelism };
			const uint32_t blocks_per_slice{ blocks_per_lane / Sync_Points };

			// first slice
			for (uint32_t lane = 0; lane < params.parallelism; ++lane) {
				const uint32_t lane_start_idx{ lane * blocks_per_lane };
				std::array<uint32_t, 2> extension{ 0, lane };
				std::array<std::byte, Initial_Hash_Size + sizeof(extension)> input{};
				std::copy(hash.begin(), hash.end(), input.begin());

				// first block
				std::memcpy(input.data() + Initial_Hash_Size, extension.data(), sizeof(extension));
				blake2b::hash(std::span<std::byte>{ memory[lane_start_idx] }, input);

				// second block
				extension[0] = 1;
				std::memcpy(input.data() + Initial_Hash_Size, extension.data(), sizeof(extension));
				blake2b::hash(std::span<std::byte>{ memory[lane_start_idx + 1] }, input);

				// third block is always mix of first two
				mixBlocks(memory[lane_start_idx + 2], memory[lane_start_idx], memory[lane_start_idx + 1], false);

				// calculate next blocks in slice
				for (uint64_t prev_idx = 2, idx = 3; idx < blocks_per_slice; ++prev_idx, ++idx) {
					const auto cur_idx{ lane_start_idx + idx };
					const auto ref_length{ prev_idx }; // for slice 0 and iteration 0 it will always be up to previous block
					const auto J1{ bytes_cast<uint64_t>(memory[prev_idx]) & 0x00000000ffffffff };
					const auto x{ (J1 * J1) >> 32 };
					const auto y{ (ref_length * x) >> 32 };
					const auto z{ ref_length - 1 - y };
					const auto ref_index{ z };

					mixBlocks(memory[cur_idx], memory[prev_idx], memory[ref_index], false);
				}
			}

			// next slices
			for (uint32_t slice = 1; slice < Sync_Points; ++slice) {
				for (uint32_t lane = 0; lane < params.parallelism; ++lane) {
					const auto lane_start_idx{ lane * blocks_per_lane };

					for (uint32_t idx = 0; idx < blocks_per_slice; ++idx) {
						const auto cur_idx{ lane_start_idx + slice * blocks_per_slice + idx };
						const auto prev_idx{ cur_idx - 1 };
						const auto J1{ bytes_cast<uint64_t>(memory[prev_idx]) & 0x00000000ffffffff };
						const auto J2{ bytes_cast<uint64_t>(memory[prev_idx]) >> 32 };
						const auto ref_lane{ J2 % params.parallelism };

						// if block from different lane, we can pick max s finished slices * blocks_per_slice
						// otherwise we can pick all blocks up to prev_idx
						const auto ref_length{ (ref_lane == lane) ? prev_idx - lane_start_idx : blocks_per_slice * slice };
						const auto x{ (J1 * J1) >> 32 };
						const auto y{ (ref_length * x) >> 32 };
						const auto z{ ref_length - 1 - y };
						const auto ref_index{ ref_lane * blocks_per_lane + z }; // we pick z' block from a ref_lane

						mixBlocks(memory[cur_idx], memory[prev_idx], memory[ref_index], false);
					}
				}
			}
		}


		/*
		* Performs all except first iterations of filling memory in two steps:
		* 1. For every lane calculate first block.
		* 2. Calculate all other blocks.
		*
		* The main difference between this and first iteration is that referenced block will be the one of
		* the block from previous iteration or the block from current iteration. It all depends if referenced block
		* lies in rolling window of size equal to three full slices.
		*
		* For more details check https://github.com/P-H-C/phc-winner-argon2/blob/master/argon2-specs.pdf section 3.2 and 3.3.
		*/
		void makeSecondPass(Memory& memory, const Params& params) {
			const uint32_t blocks_per_lane{ params.memory_blocks / params.parallelism };
			const uint32_t blocks_per_slice{ blocks_per_lane / Sync_Points };

			for (uint32_t round = 1; round < params.iterations; ++round) {
				// calculate first block for every lane
				for (uint32_t lane = 0; lane < params.parallelism; ++lane) {
					const auto lane_start_idx{ blocks_per_lane * lane };
					const auto prev_idx{ lane_start_idx + blocks_per_lane - 1 };
					const auto J1{ bytes_cast<uint64_t>(memory[prev_idx]) & 0x00000000ffffffff };
					const auto J2{ bytes_cast<uint64_t>(memory[prev_idx]) >> 32 };
					const auto ref_lane{ J2 % params.parallelism };

					// we can pick up to (sync_points - 1) 3 * blocks_per_slice blocks
					const auto ref_length{ blocks_per_lane - blocks_per_slice - (ref_lane == lane) };
					const auto x{ (J1 * J1) >> 32 };
					const auto y{ (ref_length * x) >> 32 };
					const auto z{ ref_length - 1 - y };
					const auto shift_index{ blocks_per_slice };
					const auto ref_index{ ref_lane * blocks_per_lane + shift_index + z }; // no need to modulo by blocks_per_lane because it will always be less than blocks_per_lane

					mixBlocks(memory[lane_start_idx], memory[prev_idx], memory[ref_index], true);
				}

				// calculate all next blocks in every lane
				for (uint32_t slice = 0; slice < Sync_Points; ++slice) {
					for (uint32_t lane = 0; lane < params.parallelism; ++lane) {
						const auto lane_start_idx{ blocks_per_lane * lane + blocks_per_slice * slice };

						// idx = (slice == 0) because for slice 0, we already calculated first block
						for (uint32_t idx = (slice == 0); idx < blocks_per_slice; ++idx) {
							const auto cur_idx{ lane_start_idx + idx };
							const auto prev_idx{ cur_idx - 1 };
							const auto J1{ bytes_cast<uint64_t>(memory[prev_idx]) & 0x00000000ffffffff };
							const auto J2{ bytes_cast<uint64_t>(memory[prev_idx]) >> 32 };
							const auto ref_lane{ J2 % params.parallelism };
							auto ref_length{ blocks_per_lane - blocks_per_slice }; // we can pick up to (sync_points - 1) * blocks_per_slice blocks
							if (ref_lane == lane) {
								ref_length += idx - 1; // plus up to currently built blocks
							}
							const auto x{ (J1 * J1) >> 32 };
							const auto y{ (ref_length * x) >> 32 };
							const auto z{ ref_length - 1 - y };
							auto shift_index{ blocks_per_slice * ((slice + 1) % Sync_Points) }; // start at beginning of (slice - 3; roll over if overflows)
							shift_index = (shift_index + z) % blocks_per_lane; // this may overflow blocks_per_lane so have to be rolled over (% blocks_per_lane)
							const auto ref_index{ ref_lane * blocks_per_lane + shift_index };

							mixBlocks(memory[cur_idx], memory[prev_idx], memory[ref_index], true);
						}
					}
				}
			}
		}

		// Calculates current block based on previous and referenced random block.
		// If its first iteration xor_blocks should be false, otherwise should be true and will perform xor operation with overwritten block.
		// The mixing function refers to https://github.com/P-H-C/phc-winner-argon2/blob/master/argon2-specs.pdf section 3.4.
		void mixBlocks(Block& cur_block, const Block& prev_block, const Block& ref_block, const bool xor_blocks) {
			static constexpr uint32_t Uint64_Per_Block{ Block_Size / sizeof(uint64_t) };
			using Uint64Block = std::array<uint64_t, Uint64_Per_Block>;

			const auto prev_block_64{ std::bit_cast<Uint64Block>(prev_block) };
			const auto ref_block_64{ std::bit_cast<Uint64Block>(ref_block) };
			auto cur_block_64{ std::bit_cast<Uint64Block>(cur_block) };

			Uint64Block tmp_block_64{};

			for (uint32_t i = 0; i < Uint64_Per_Block; ++i) {
				// R = X ⊕ Y
				tmp_block_64[i] = prev_block_64[i] ^ ref_block_64[i];

				// R' = X ⊕ Y [⊕ overwritten block] - this is the "R" used in final result
				cur_block_64[i] = prev_block_64[i] ^ ref_block_64[i] ^ (xor_blocks * cur_block_64[i]);
			}

			// Apply blake2b "rowwise", ie. elements (0,1,...,15), then (16,17,...,31) ... finally (112,113,...,127).
			// Each two adjacent elements are viewed as "16-byte registers" mentioned in the paper.
			for (uint32_t i = 0; i < 8; ++i) {
				const auto offset{ 16 * i };
				const auto offset2{ 16 * (i + 1) };

				blake2b::round(std::span<uint64_t, 16>(tmp_block_64.begin() + offset, tmp_block_64.begin() + offset2));
			}

			// Apply blake2b "columnwise", ie. elements (0,1,16,17,...,112,113), then (2,3,18,19,...,114,115) ... finally (14,15,30,31,...,126,127).
			for (uint32_t i = 0; i < 8; ++i) { 
				// shuffle elements to form column
				auto column = std::array<uint64_t, 16>{
					tmp_block_64[2 * i], tmp_block_64[2 * i + 1], tmp_block_64[2 * i + 16], tmp_block_64[2 * i + 17],
					tmp_block_64[2 * i + 32], tmp_block_64[2 * i + 33], tmp_block_64[2 * i + 48], tmp_block_64[2 * i + 49],
					tmp_block_64[2 * i + 64], tmp_block_64[2 * i + 65], tmp_block_64[2 * i + 80], tmp_block_64[2 * i + 81],
					tmp_block_64[2 * i + 96], tmp_block_64[2 * i + 97], tmp_block_64[2 * i + 112], tmp_block_64[2 * i + 113]
				};

				blake2b::round(column);

				// copy state back into original block 
				for (uint32_t j = 0; j < 8; ++j) {
					tmp_block_64[2 * i + 16 * j] = column[2 * j];
					tmp_block_64[2 * i + 1 + 16 * j] = column[2 * j + 1];
				}
			}

			// "Finally, G outputs Z ⊕ R"
			for (uint32_t i = 0; i < Uint64_Per_Block; ++i) {
				cur_block_64[i] ^= tmp_block_64[i];
			}

			cur_block = std::bit_cast<Block>(cur_block_64);
		}
	}
}
#include <array>
#include <bit>
#include <format>

#include "aes.hpp"
#include "cast.hpp"

namespace modernRX::aes {
    namespace {
		// state0, state1, state2, state3 = Blake2b-512("RandomX AesHash1R state")
		// state0 = 0d 2c b5 92 de 56 a8 9f 47 db 82 cc ad 3a 98 d7
		// state1 = 6e 99 8d 33 98 b7 c7 15 5a 12 9e f5 57 80 e7 ac
		// state2 = 17 00 77 6a d0 c7 62 ae 6b 50 79 50 e4 7c a0 e8
		// state3 = 0c 24 0a 63 8d 82 ad 07 05 00 a1 79 48 49 99 7e
		constexpr auto initial_state{ byte_array(
			0x0d, 0x2c, 0xb5, 0x92, 0xde, 0x56, 0xa8, 0x9f, 0x47, 0xdb, 0x82, 0xcc, 0xad, 0x3a, 0x98, 0xd7,
			0x6e, 0x99, 0x8d, 0x33, 0x98, 0xb7, 0xc7, 0x15, 0x5a, 0x12, 0x9e, 0xf5, 0x57, 0x80, 0xe7, 0xac,
			0x17, 0x00, 0x77, 0x6a, 0xd0, 0xc7, 0x62, 0xae, 0x6b, 0x50, 0x79, 0x50, 0xe4, 0x7c, 0xa0, 0xe8,
			0x0c, 0x24, 0x0a, 0x63, 0x8d, 0x82, 0xad, 0x07, 0x05, 0x00, 0xa1, 0x79, 0x48, 0x49, 0x99, 0x7e
		) };

		// xkey0, xkey1 = Blake2b-256("RandomX AesHash1R xkeys")
		// xkey0 = 89 83 fa f6 9f 94 24 8b bf 56 dc 90 01 02 89 06
		// xkey1 = d1 63 b2 61 3c e0 f4 51 c6 43 10 ee 9b f9 18 ed
		//
		// Initial state and keys are treated as four 4-byte integers (in little-endian byte order).
		constexpr std::array<std::array<uint32_t, 4>, 2> keys{
			std::array<uint32_t, 4>{ 0xf6fa8389, 0x8b24949f, 0x90dc56bf, 0x06890201 },
			std::array<uint32_t, 4>{ 0x61b263d1, 0x51f4e03c, 0xee1043c6, 0xed18f99b },
		};
    }

	void hash1R(std::span<std::byte, 64> output, const_span<std::byte> input) {
		if (input.size() % 64 != 0) {
			throw std::format("invalid input size: {}", input.size());
		}

		std::array<std::array<uint32_t, 4>, 4> state{};
		std::memcpy(state.data(), initial_state.data(), sizeof(initial_state));

		for (uint64_t i = 0; i < input.size(); i += 64) {
			const auto offset{ input.data() + i };

			encode(state[0], span_cast<uint32_t, 4>(offset));
			decode(state[1], span_cast<uint32_t, 4>(offset + 16));
			encode(state[2], span_cast<uint32_t, 4>(offset + 32));
			decode(state[3], span_cast<uint32_t, 4>(offset + 48));
		}

		encode(state[0], keys[0]);
		decode(state[1], keys[0]);
		encode(state[2], keys[0]);
		decode(state[3], keys[0]);

		encode(state[0], keys[1]);
		decode(state[1], keys[1]);
		encode(state[2], keys[1]);
		decode(state[3], keys[1]);

		std::memcpy(output.data(), state[0].data(), 16);
		std::memcpy(output.data() + 16, state[1].data(), 16);
		std::memcpy(output.data() + 32, state[2].data(), 16);
		std::memcpy(output.data() + 48, state[3].data(), 16);
	}
}

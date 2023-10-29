#include <format>

#include "aes.hpp"
#include "aes4rrandom.hpp"
#include "cast.hpp"

namespace modernRX::aes {
	namespace {
		// key0, key1, key2, key3 = Blake2b-512("RandomX AesGenerator4R keys 0-3")
		// key4, key5, key6, key7 = Blake2b-512("RandomX AesGenerator4R keys 4-7")
		// key0 = dd aa 21 64 db 3d 83 d1 2b 6d 54 2f 3f d2 e5 99
		// key1 = 50 34 0e b2 55 3f 91 b6 53 9d f7 06 e5 cd df a5
		// key2 = 04 d9 3e 5c af 7b 5e 51 9f 67 a4 0a bf 02 1c 17
		// key3 = 63 37 62 85 08 5d 8f e7 85 37 67 cd 91 d2 de d8
		// key4 = 73 6f 82 b5 a6 a7 d6 e3 6d 8b 51 3d b4 ff 9e 22
		// key5 = f3 6b 56 c7 d9 b3 10 9c 4e 4d 02 e9 d2 b7 72 b2
		// key6 = e7 c9 73 f2 8b a3 65 f7 0a 66 a9 2b a7 ef 3b f6
		// key7 = 09 d6 7c 7a de 39 58 91 fd d1 06 0c 2d 76 b0 c0
		//
		// Every key is treated as four 4-byte integers (in little-endian byte order).
		constexpr std::array<std::array<uint32_t, 4>, 8> keys{
			std::array<uint32_t, 4>{ 0x6421aadd, 0xd1833ddb, 0x2f546d2b, 0x99e5d23f },
			std::array<uint32_t, 4>{ 0xb20e3450, 0xb6913f55, 0x06f79d53, 0xa5dfcde5 },
			std::array<uint32_t, 4>{ 0x5c3ed904, 0x515e7baf, 0x0aa4679f, 0x171c02bf },
			std::array<uint32_t, 4>{ 0x85623763, 0xe78f5d08, 0xcd673785, 0xd8ded291 },
			std::array<uint32_t, 4>{ 0xb5826f73, 0xe3d6a7a6, 0x3d518b6d, 0x229effb4 },
			std::array<uint32_t, 4>{ 0xc7566bf3, 0x9c10b3d9, 0xe9024d4e, 0xb272b7d2 },
			std::array<uint32_t, 4>{ 0xf273c9e7, 0xf765a38b, 0x2ba9660a, 0xf63befa7 },
			std::array<uint32_t, 4>{ 0x7a7cd609, 0x915839de, 0x0c06d1fd, 0xc0b0762d },
		};

	}

	void fill4R(std::span<std::byte> output, std::span<std::byte, 64> seed) {
		if (output.size() % 64 != 0) {
			throw std::format("invalid output size: {}", output.size());
		}

		auto state{ span_cast<std::array<uint32_t, 4>, 4>(seed.data()) };

		for (size_t i = 0; i < output.size(); i += 64) {
			decode(state[0], keys[0]);
			encode(state[1], keys[0]);
			decode(state[2], keys[4]);
			encode(state[3], keys[4]);

			decode(state[0], keys[1]);
			encode(state[1], keys[1]);
			decode(state[2], keys[5]);
			encode(state[3], keys[5]);

			decode(state[0], keys[2]);
			encode(state[1], keys[2]);
			decode(state[2], keys[6]);
			encode(state[3], keys[6]);

			decode(state[0], keys[3]);
			encode(state[1], keys[3]);
			decode(state[2], keys[7]);
			encode(state[3], keys[7]);

			std::memcpy(output.data() + i, state.data(), 64);
		}
	}
}

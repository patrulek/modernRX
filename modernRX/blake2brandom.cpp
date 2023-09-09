#include <bit>

#include "blake2brandom.hpp"

namespace modernRX::blake2b {
	Random::Random(const_span<std::byte> seed, const uint32_t nonce) noexcept {
		static constexpr uint32_t Max_Seed_Size{ 60 };
		const auto size{ std::min<size_t>(Max_Seed_Size, seed.size()) };

		std::memcpy(state.data(), seed.data(), size);
		std::memcpy(state.data() + size, &nonce, sizeof(uint32_t));

		// Always rehash at initialization.
		rehashIfNeeded(state.size() + 1); 
	}

	uint8_t Random::getUint8() noexcept {
		rehashIfNeeded(1);
		return static_cast<uint8_t>(state[position++]);
	}

	uint32_t Random::getUint32() noexcept {
		rehashIfNeeded(4);
		const_array<std::byte, 4> u32_bytes{ state[position], state[position + 1], state[position + 2], state[position + 3] };
		position += 4;
		return std::bit_cast<uint32_t>(u32_bytes);
	}

	void Random::rehashIfNeeded(const size_t bytes_needed) noexcept {
		if (position + bytes_needed > state.size()) {
			hash(state, state);
			position = 0;
		}
	}
}
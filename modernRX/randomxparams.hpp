#pragma once

#include <array>
#include <string_view>

#include "argon2d.hpp" // TODO: this should be decoupled


namespace modernRX {
	// RandomX\x03
	inline constexpr std::array<char, 8> Rx_Argon2d_Salt{ 0x52, 0x61, 0x6e, 0x64, 0x6f, 0x6d, 0x58, 0x03 };
	inline constexpr std::array<char, 4> Rx_Argon2d_Password{ 0x00, 0x00, 0x00, 0x00 };
	inline constexpr uint32_t Rx_Superscalar_Latency{ 170 };
	inline constexpr uint32_t Rx_Superscalar_Op_Max_Latency{ 4 };
	inline constexpr uint32_t Rx_Superscalar_Max_Program_Size{ 3 * Rx_Superscalar_Latency + 2 };
	inline constexpr uint32_t Rx_Max_Schedule_Cycle = Rx_Superscalar_Latency + Rx_Superscalar_Op_Max_Latency;
	inline constexpr uint32_t Rx_Superscalar_Programs_Count{ 8 };

	inline constexpr argon2d::Params Rx_Argon2d_Params{
		.secret{ std::span<std::byte>() },
		.data{ std::span<std::byte>() },
		.parallelism{ 1 },
		.tag_length{ 0 },
		.memory_blocks{ 262144 },
		.iterations{ 3 },
		.type{ 0 },
		.version{ 0x13 },
	};

	static_assert(Rx_Argon2d_Params.secret.size() <= std::numeric_limits<uint32_t>::max());
	static_assert(Rx_Argon2d_Params.data.size() <= std::numeric_limits<uint32_t>::max());
	static_assert(Rx_Argon2d_Params.parallelism - 1 <= argon2d::Max_Parallelism - 1);
	static_assert(Rx_Argon2d_Params.tag_length <= std::numeric_limits<uint32_t>::max()); // Its different from a Argon2d paper and may be any value, as its not needed to calculate final hash.
	static_assert(Rx_Argon2d_Params.memory_blocks - argon2d::Min_Memory_Blocks <= std::numeric_limits<uint32_t>::max() - argon2d::Min_Memory_Blocks);
	static_assert(Rx_Argon2d_Params.iterations - 1 <= std::numeric_limits<uint32_t>::max() - 1);
	static_assert(Rx_Argon2d_Params.type == 0);
	static_assert(Rx_Argon2d_Params.version == 0x13);
}
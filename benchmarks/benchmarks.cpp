#include <print>
#include <chrono>
#include <string>
#include <string_view>
#include <print>

#include "blake2b.hpp"
#include "argon2d.hpp"

static int benchNo = 0;
static int skipped = 0;

template<typename FUNC, int Bytes = 0>
void runBenchmark(const std::string_view name, const bool condition, FUNC f) {
	constexpr std::string_view fmt1{ "[{:2d}] {:40s} ... {:s}" };
	constexpr std::string_view fmt2{ "[{:2d}] {:40s}\n-----\nElapsed time\tAvg time\tThroughput\n{:>11.3f}s\t{:>6.3f}us\t{:>6.1f}{:s}\n" };

	if (condition) {
		constexpr int ITERATIONS = 100'000;
		constexpr auto sumBytes = static_cast<double>(ITERATIONS * Bytes);
		constexpr std::array<std::string_view, 5> units{ "B/s", "KB/s", "MB/s", "GB/s", "TB/s" };

		std::vector<uint64_t> dur; dur.reserve(ITERATIONS);

		auto startB = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < ITERATIONS; i++) {
			f();
		}
		auto stopB = std::chrono::high_resolution_clock::now();
		auto elapsed = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(stopB - startB).count());

		auto sumBytesPerSecond = sumBytes / (elapsed / 1000000.0);
		auto unit = 0;
		while (sumBytesPerSecond > 1024) {
			sumBytesPerSecond /= 1024.0;
			unit++;
		}

		std::println(fmt2, benchNo++, name, elapsed / 1000000.0, elapsed / static_cast<double>(ITERATIONS), sumBytesPerSecond, units[unit]);
	}
	else {
		std::println(fmt1, benchNo++, name, "SKIPPED");
		skipped++;
	}
}

int main() {
	using namespace modernRX;

	auto data = std::bit_cast<std::array<std::byte, 4>>(std::array<char, 4>("abc"));
	constexpr auto digest_size = 64;
	std::array<std::byte, digest_size> hash;
	auto b1 = [&data, &hash]() {
		blake2b::hash(hash, std::span<std::byte>(data.data(), data.size() - 1), std::span<std::byte>());
	};
	runBenchmark<decltype(b1), 3>("Blake2b::hash (3 bytes)", true, b1);


	auto data2 = std::bit_cast<std::array<std::byte, 58>>(std::array<char, 58>{ "12392i4392jru1nf;j1nfd1nfp1uedhfpuedhfie2dnflk2edfhjnlehf" });
	auto b2 = [&data2, &hash]() {
		blake2b::hash(hash, std::span<std::byte>(data2.data(), data2.size() - 1), std::span<std::byte>());
	};
	runBenchmark<decltype(b2), 57>("Blake2b::hash (57 bytes)", true, b2);

	std::vector<std::byte> data4(4 * 1024, std::byte('x'));
	auto b4 = [&data4, &hash]() {
		blake2b::hash(hash, std::span<std::byte>(data4.data(), data4.size()), std::span<std::byte>());
	};
	runBenchmark<decltype(b4), 4 * 1024>("Blake2b::hash (4 kilobytes)", true, b4);

	std::vector<std::byte> data3(16 * 1024, std::byte('x'));
	auto b3 = [&data3, &hash]() {
		blake2b::hash(hash, std::span<std::byte>(data3.data(), data3.size()), std::span<std::byte>());
	};
	runBenchmark<decltype(b3), 16 * 1024>("Blake2b::hash (16 kilobytes)", true, b3);

	std::vector<std::byte> hash_long(1024, std::byte(0));
	auto b5 = [&hash_long, &data]() {
		argon2d::blake2b::hash(hash_long, data);
	};
	runBenchmark<decltype(b5), 3>("Argon2d::Blake2b::hash (3 bytes input, 1024 bytes output)", true, b5);

	auto b6 = [&hash_long, &data4]() {
		argon2d::blake2b::hash(hash_long, data4);
	};
	runBenchmark<decltype(b6), 4096>("Argon2d::Blake2b::hash (4096 bytes input, 1024 bytes output)", true, b6);

	return 0;
}
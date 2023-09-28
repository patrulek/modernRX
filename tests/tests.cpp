#include <functional>
#include <print>
#include <source_location>

#include "aes1rhash.hpp"
#include "aes1rrandom.hpp"
#include "aes4rrandom.hpp"
#include "argon2d.hpp"
#include "blake2b.hpp"
#include "blake2brandom.hpp"
#include "cast.hpp"
#include "dataset.hpp"
#include "hasher.hpp"
#include "jitcompiler.hpp"
#include "randomxparams.hpp"
#include "reciprocal.hpp"
#include "superscalar.hpp"


using namespace modernRX;

static std::array<char, 13> test_key{ "test key 000" };
static std::array<char, 15> test_input{ "This is a test" };
static std::array<char, 27> test_input2{ "Lorem ipsum dolor sit amet" };

static int testNo{ 0 };

static auto key{ span_cast<std::byte, test_key.size() - 1>(test_key) };
static auto input{ span_cast<std::byte, test_input.size() - 1>(test_input) };
static auto input2{ span_cast<std::byte, test_input2.size() - 1>(test_input2) };

static std::array<std::byte, 76> block_template{ byte_array(
	0x07, 0x07, 0xf7, 0xa4, 0xf0, 0xd6, 0x05, 0xb3, 0x03, 0x26, 0x08, 0x16, 0xba, 0x3f, 0x10, 0x90, 0x2e, 0x1a, 0x14,
	0x5a, 0xc5, 0xfa, 0xd3, 0xaa, 0x3a, 0xf6, 0xea, 0x44, 0xc1, 0x18, 0x69, 0xdc, 0x4f, 0x85, 0x3f, 0x00, 0x2b, 0x2e,
	0xea, 0x00, 0x00, 0x00, 0x00, 0x77, 0xb2, 0x06, 0xa0, 0x2c, 0xa5, 0xb1, 0xd4, 0xce, 0x6b, 0xbf, 0xdf, 0x0a, 0xca,
	0xc3, 0x8b, 0xde, 0xd3, 0x4d, 0x2d, 0xcd, 0xee, 0xf9, 0x5c, 0xd2, 0x0c, 0xef, 0xc1, 0x2f, 0x61, 0xd5, 0x61, 0x09
) };

void testAssert(const bool condition, const std::source_location& location = std::source_location::current()) {
	if (!condition) {
		throw location;
    }
}

void runTest(const std::string_view name, const bool condition, std::function<void()> test) {
	constexpr std::string_view fmt_header{ "[{:2d}] {:40s} ... " };

	std::print(fmt_header, testNo++, name);

	if (!condition) {
		std::println("Skipped");
		return;
	}

	const auto startT{ std::chrono::high_resolution_clock::now() };

	try {
		test();
		std::print("Passed");
	} catch (const std::source_location& location) {
		std::string_view file_name { location.file_name() };
		file_name = file_name.substr(file_name.find_last_of("\\/") + 1);

		std::print("Failed at {}:{}", file_name, location.line());
	} catch (...) {
        std::print("Unexpected error");
    }

	const auto endT{ std::chrono::high_resolution_clock::now() };
	const auto elapsed{ static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(endT - startT).count()) / 1'000'000'000.0 };

	if (elapsed < 0.001) {
		std::println(" (<1ms)");
	} else {
		std::println(" ({:.3f}s)", elapsed);
	}
}

void testBlake2bHash();
void testArgon2dBlake2bHash();
void testArgon2dFillMemory();
void testAesGenerator1RFill();
void testAesGenerator4RFill();
void testAesHash1R();
void testBlake2bRandom();
void testSuperscalarGenerate();
void testReciprocal();
void testDatasetGenerate();
void testHasher();


int main() {
	runTest("Blake2b::hash", true, testBlake2bHash);
	runTest("Argon2d::Blake2b::hash", true, testArgon2dBlake2bHash);
	runTest("Argon2d::fillMemory", true, testArgon2dFillMemory);
	runTest("AesGenerator1R::fill", true, testAesGenerator1RFill);
	runTest("AesGenerator4R::fill", true, testAesGenerator4RFill);
	runTest("AesHash1R", true, testAesHash1R);
	runTest("Blake2brandom::get", true, testBlake2bRandom);
	runTest("Reciprocal", true, testReciprocal);
	runTest("Superscalar::generate", true, testSuperscalarGenerate);
	runTest("Dataset::generate", true, testDatasetGenerate);
	runTest("Hasher::run", true, testHasher);
}


void testBlake2bHash() {
	std::array<std::byte, 64> hash;
	auto data = byte_array('a', 'b', 'c');

	auto expected = byte_array(
		0xBA, 0x80, 0xA5, 0x3F, 0x98, 0x1C, 0x4D, 0x0D, 0x6A, 0x27, 0x97, 0xB6, 0x9F, 0x12, 0xF6, 0xE9,
		0x4C, 0x21, 0x2F, 0x14, 0x68, 0x5A, 0xC4, 0xB7, 0x4B, 0x12, 0xBB, 0x6F, 0xDB, 0xFF, 0xA2, 0xD1,
		0x7D, 0x87, 0xC5, 0x39, 0x2A, 0xAB, 0x79, 0x2D, 0xC2, 0x52, 0xD5, 0xDE, 0x45, 0x33, 0xCC, 0x95,
		0x18, 0xD3, 0x8A, 0xA8, 0xDB, 0xF1, 0x92, 0x5A, 0xB9, 0x23, 0x86, 0xED, 0xD4, 0x00, 0x99, 0x23
	);

	blake2b::hash(hash, span_cast<std::byte>(data));
	testAssert(hash == expected);

	auto data2 = byte_array(
		0x3c, 0xaf, 0x6a, 0x0f, 0x45, 0x51, 0xdc, 0xd8, 0xc4, 0x09, 0xa5, 0xd5, 0x04, 0xe0, 0x01, 0xee,
		0x10, 0x22, 0x5d, 0x78, 0x0a, 0xf8, 0x56, 0x0d, 0x31, 0xc5, 0x80, 0x16, 0x16, 0xe0, 0x25, 0x64,
		0x6c, 0x0c, 0x00, 0x08, 0xb9, 0x16, 0x9f, 0x86, 0x31, 0x06, 0xa7, 0x72, 0x68, 0xf0, 0xc8, 0x4a,
		0xac, 0x1d, 0x89, 0xe7, 0x9b, 0x37, 0x6b, 0x91, 0xa0, 0x7b, 0xe8, 0x42, 0xa5, 0x37, 0x71, 0x53
	);

	expected = byte_array(
		0x76, 0x19, 0x38, 0x88, 0xb7, 0x51, 0xab, 0xd1, 0x6f, 0xcc, 0xcb, 0xf2, 0xf9, 0xc7, 0x8e, 0x15,
		0xfc, 0x20, 0xc9, 0xe6, 0xab, 0x32, 0xc1, 0xa1, 0xa9, 0x0b, 0x19, 0xfe, 0x14, 0x19, 0x03, 0x96,
		0xc1, 0xa0, 0xe9, 0xea, 0x21, 0x95, 0x31, 0xbf, 0xbf, 0xb1, 0x55, 0x68, 0xef, 0x3a, 0x1c, 0x58,
		0xa8, 0x1e, 0x95, 0x7a, 0x09, 0xfb, 0xad, 0x42, 0x56, 0x75, 0x7e, 0xcf, 0x1b, 0x33, 0xda, 0x49
	);

	blake2b::hash(hash, data2);
	testAssert(hash == expected);


	std::array<std::byte, 256> data3{};
	data3.fill(std::byte{ 0x37 });

	expected = byte_array(
		0x46, 0xb1, 0x1e, 0x36, 0xbf, 0x69, 0xf3, 0x92, 0x44, 0xe9, 0x24, 0xe9, 0x00, 0x4d, 0xe2, 0xf3,
		0x92, 0xae, 0x48, 0x21, 0x59, 0xfc, 0x97, 0x2b, 0xec, 0xbe, 0x17, 0x94, 0xe8, 0x69, 0x86, 0x6f,
		0xd3, 0x98, 0x8f, 0xe3, 0xd2, 0x8b, 0xe8, 0x07, 0x91, 0x55, 0x3a, 0x6c, 0x08, 0xab, 0xb4, 0x71,
		0xda, 0x8b, 0xe2, 0x27, 0x56, 0xb3, 0x70, 0xea, 0x2a, 0x1a, 0xcc, 0x6d, 0xea, 0xcf, 0x2f, 0xac
	);

	blake2b::hash(hash, data3);
	testAssert(hash == expected);
}

void testArgon2dBlake2bHash() {
	constexpr uint32_t digest_size{ 1024 };
	std::vector<std::byte> hash(digest_size);

	auto data = byte_array(
		0x34, 0x05, 0x75, 0xf8, 0x57, 0x95, 0xc2, 0x0e, 0xd0, 0xe0, 0x7f, 0x73, 0x56, 0xa0, 0x2c, 0xf5,
		0x50, 0x18, 0x56, 0x7f, 0x6a, 0xd3, 0x4f, 0x24, 0x59, 0x0f, 0xf8, 0xf8, 0xb1, 0x2f, 0xfa, 0xaa,
		0xd9, 0x34, 0x8a, 0x30, 0x70, 0xf3, 0xf5, 0x89, 0xe4, 0xa2, 0xb4, 0x18, 0x7c, 0xd2, 0x67, 0xfc,
		0x04, 0x98, 0x08, 0x0f, 0xb1, 0xe0, 0x77, 0xc4, 0xfc, 0x22, 0x06, 0x73, 0x2d, 0x0c, 0x14, 0xb2,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	);

	auto expected = byte_vector(
		0x25, 0x8c, 0x44, 0x4b, 0x5b, 0xa3, 0x55, 0x6f, 0x90, 0x2b, 0xc2, 0x5f, 0xfa, 0x6f, 0x09, 0xb7,
		0x6c, 0xbc, 0x44, 0xc0, 0xb3, 0xcd, 0x09, 0x9d, 0xf4, 0xdf, 0xc9, 0xf5, 0x63, 0xc2, 0x17, 0x53,
		0xba, 0xaa, 0x24, 0xa3, 0x72, 0xb8, 0x48, 0x5b, 0xac, 0x3c, 0x79, 0xac, 0x22, 0x52, 0x0f, 0x38,
		0x5c, 0xd5, 0x43, 0x9a, 0x1b, 0x27, 0xd6, 0xfe, 0xac, 0xd3, 0x59, 0xc6, 0x92, 0xb3, 0x3e, 0x14,
		0x62, 0x11, 0xe5, 0xb1, 0x94, 0x95, 0x27, 0x23, 0xae, 0x7e, 0x3c, 0x04, 0xb2, 0x51, 0xab, 0xda,
		0xff, 0xf0, 0xe0, 0xf2, 0x4a, 0x3a, 0x7c, 0x74, 0xe0, 0x91, 0xfb, 0x94, 0x89, 0xdd, 0xbb, 0xd2,
		0x47, 0xb7, 0xd8, 0x72, 0xa4, 0x37, 0x00, 0xb6, 0x11, 0xe2, 0x62, 0xb3, 0xd6, 0x04, 0x51, 0x23,
		0x4b, 0xf5, 0xc9, 0xdb, 0x7f, 0xfc, 0x48, 0xda, 0x1f, 0x8f, 0x35, 0xf6, 0x04, 0xab, 0xb1, 0xdc,
		0x9f, 0x27, 0x42, 0x97, 0xaa, 0x7c, 0x72, 0x25, 0x62, 0xd5, 0x18, 0x3c, 0xeb, 0x74, 0x65, 0x83,
		0x72, 0x32, 0xae, 0xaa, 0x83, 0xa4, 0x7b, 0x55, 0x33, 0x31, 0x54, 0xcf, 0xd5, 0x86, 0xcf, 0x79,
		0x77, 0x4b, 0x75, 0xbe, 0xf1, 0x20, 0x52, 0x5d, 0x5d, 0x7f, 0xd7, 0x09, 0xcc, 0x42, 0xd6, 0xc0,
		0x07, 0xda, 0xac, 0x7d, 0x82, 0x06, 0xf6, 0x5f, 0x8e, 0x9d, 0x5b, 0x9a, 0x3b, 0xd5, 0x43, 0x68,
		0xc9, 0x11, 0x12, 0x07, 0xd8, 0x6a, 0xab, 0x38, 0x2a, 0x75, 0x7d, 0xfd, 0x47, 0x23, 0x57, 0x3e,
		0x55, 0x05, 0x86, 0x5b, 0x61, 0x6d, 0x08, 0x79, 0xbc, 0x47, 0x77, 0xf5, 0x2e, 0xf5, 0x04, 0xd3,
		0x0d, 0x91, 0x9e, 0x5a, 0xbd, 0xc0, 0xcb, 0x24, 0xcb, 0xeb, 0x77, 0x56, 0xea, 0xc7, 0x5a, 0x66,
		0x3b, 0x5e, 0x18, 0x01, 0xfa, 0x79, 0x9d, 0x01, 0xbf, 0xe8, 0xe2, 0x62, 0x8b, 0x4b, 0x8f, 0xa3,
		0x15, 0x11, 0xba, 0xa8, 0xe0, 0x6e, 0x38, 0x60, 0x83, 0x37, 0x27, 0xc1, 0x2b, 0xcb, 0x13, 0x1e,
		0xbf, 0x4d, 0x27, 0x93, 0x7c, 0x69, 0xdf, 0x23, 0xd8, 0x94, 0xa5, 0xaa, 0x04, 0x30, 0xe4, 0x03,
		0x92, 0x2c, 0xb9, 0xfb, 0xb3, 0x73, 0xf7, 0x22, 0x87, 0xe6, 0xe5, 0x12, 0x19, 0xfc, 0x12, 0x42,
		0xbc, 0xd2, 0x73, 0x4f, 0xa6, 0x51, 0x44, 0x8f, 0x0f, 0x31, 0x1c, 0xf2, 0x3c, 0xf3, 0xe5, 0x5a,
		0xda, 0x31, 0x0a, 0x2c, 0x4a, 0x38, 0xb7, 0x7c, 0xb9, 0x0c, 0x33, 0xf2, 0x88, 0xd7, 0x5f, 0xb2,
		0x34, 0xab, 0xb8, 0xb3, 0x25, 0x36, 0x5a, 0x66, 0x85, 0xd5, 0x0f, 0x87, 0x47, 0x42, 0x92, 0x2b,
		0x66, 0x42, 0xe0, 0xb3, 0xce, 0x76, 0x0b, 0xd5, 0xc4, 0x39, 0xe5, 0xb2, 0x28, 0x26, 0xe0, 0xd8,
		0x0c, 0x53, 0x26, 0x94, 0x4a, 0x1b, 0x35, 0xef, 0x6a, 0x1d, 0xd9, 0xa6, 0x07, 0x85, 0x5a, 0xd3,
		0xb4, 0xa7, 0x7e, 0x4e, 0x5d, 0x79, 0x1d, 0xd4, 0xbc, 0xc5, 0x2c, 0x40, 0x1a, 0x90, 0xac, 0x5f,
		0x93, 0xd1, 0x1c, 0xe3, 0x13, 0xd4, 0x4a, 0xbc, 0x41, 0x3b, 0x3e, 0x4c, 0x73, 0x94, 0xe5, 0xa8,
		0x6a, 0x31, 0xba, 0xc6, 0xd0, 0x77, 0x8f, 0xa2, 0x68, 0x0f, 0xdb, 0x0f, 0x53, 0xd6, 0x65, 0x3d,
		0x5c, 0x95, 0x6e, 0x16, 0xcf, 0x45, 0xa8, 0x3f, 0x10, 0x4c, 0xcd, 0x96, 0xaf, 0xe3, 0xe8, 0xd0,
		0x57, 0xf8, 0x5d, 0x48, 0x96, 0x3e, 0x4c, 0xbc, 0x03, 0x35, 0x18, 0x81, 0xc7, 0xc8, 0x9a, 0xf0,
		0xed, 0x8f, 0x4e, 0x0e, 0xaf, 0x91, 0x1b, 0xcd, 0xf2, 0xd3, 0x42, 0xe3, 0x76, 0x42, 0x6e, 0x77,
		0x40, 0xfe, 0x08, 0xd8, 0xd8, 0x30, 0x6a, 0x42, 0xc2, 0x15, 0x2e, 0xb8, 0xfa, 0x3a, 0xfb, 0x85,
		0x10, 0xb3, 0xa4, 0xfb, 0x39, 0x51, 0xec, 0x79, 0x4a, 0xe6, 0xd8, 0x11, 0x57, 0x81, 0xe9, 0x67,
		0x7b, 0x94, 0x43, 0xf9, 0x49, 0x42, 0x36, 0xab, 0xab, 0xc1, 0x22, 0x9b, 0x58, 0x65, 0x31, 0x02,
		0x65, 0x54, 0x0a, 0xc1, 0x0c, 0xfa, 0x1b, 0x92, 0x60, 0xe7, 0xde, 0x1c, 0x99, 0x52, 0x81, 0x4d,
		0xd2, 0x47, 0x5d, 0x05, 0x17, 0x8d, 0x48, 0x73, 0x0c, 0x3f, 0x50, 0x97, 0x19, 0x88, 0x94, 0xc3,
		0x4a, 0x0c, 0x60, 0x66, 0x87, 0xb9, 0x1b, 0x7f, 0x35, 0x06, 0x5f, 0x64, 0x7a, 0xc6, 0xd7, 0xd4,
		0xf4, 0x28, 0x9c, 0xbd, 0x86, 0xb0, 0xf3, 0x65, 0x6c, 0x2a, 0xc0, 0x9d, 0x93, 0x98, 0x64, 0xc8,
		0xa7, 0xca, 0x98, 0x48, 0x82, 0xa9, 0x56, 0x61, 0xd5, 0xc0, 0x1f, 0x05, 0xbf, 0x42, 0x11, 0x2d,
		0x66, 0x63, 0xed, 0xfd, 0xb6, 0xeb, 0x7c, 0x2e, 0x5a, 0x93, 0x8c, 0xc2, 0x4a, 0x4a, 0x01, 0x64,
		0x45, 0xa9, 0x10, 0x2f, 0xb2, 0x44, 0xd4, 0x2e, 0x11, 0x5a, 0x29, 0x16, 0xc6, 0xd9, 0x8b, 0x5e,
		0x7d, 0x79, 0xd8, 0xdc, 0x62, 0x7d, 0x15, 0x63, 0xb3, 0x20, 0xba, 0x24, 0xf6, 0x1d, 0x95, 0xcd,
		0x39, 0xa1, 0x75, 0xbb, 0xd8, 0x34, 0x40, 0x66, 0x2b, 0x42, 0x89, 0x36, 0x7e, 0x3e, 0x07, 0x04,
		0x24, 0xae, 0x6e, 0x11, 0xc8, 0xce, 0x45, 0x36, 0xe5, 0x82, 0x38, 0x40, 0xb9, 0xe5, 0x79, 0xdb,
		0x25, 0x14, 0x7b, 0xf3, 0x85, 0xaf, 0x7d, 0x6b, 0xce, 0xf4, 0xf0, 0xdf, 0x5b, 0x58, 0x64, 0xd6,
		0x1b, 0x11, 0x9a, 0xec, 0x7d, 0xe1, 0x8c, 0x48, 0x61, 0xe5, 0xb7, 0xcc, 0x36, 0x37, 0x87, 0x2f,
		0x51, 0x3c, 0x29, 0xd1, 0x21, 0xab, 0x14, 0x6f, 0x11, 0x33, 0x8a, 0xa8, 0x22, 0x9e, 0x5d, 0xa9,
		0xd3, 0x7f, 0x73, 0x09, 0xe6, 0xed, 0xf3, 0x51, 0xfc, 0xdd, 0xa1, 0x35, 0xb6, 0x54, 0x63, 0xf6,
		0x5c, 0x4c, 0x1a, 0xac, 0xc4, 0x79, 0x25, 0xec, 0xbe, 0x9b, 0x83, 0xde, 0x37, 0x4a, 0xe6, 0x5b,
		0x9f, 0x79, 0x27, 0xe7, 0xbb, 0xcc, 0xe4, 0xc5, 0x20, 0xf6, 0x45, 0xea, 0x7f, 0x21, 0xa3, 0x2e,
		0xf1, 0xb4, 0x54, 0xda, 0x1a, 0xaf, 0x52, 0x5c, 0x0f, 0x7b, 0xfe, 0x72, 0xbb, 0x2f, 0x4d, 0x3f,
		0xd7, 0xdb, 0x5d, 0xbc, 0xff, 0x4f, 0x68, 0xf8, 0x80, 0x3a, 0xb8, 0x31, 0xd0, 0x43, 0xaf, 0x4a,
		0x9f, 0xb0, 0x4f, 0xd1, 0x8d, 0x6c, 0xad, 0xbe, 0x1b, 0x61, 0xf9, 0xf7, 0x63, 0x33, 0xe2, 0xb1,
		0xd9, 0xb3, 0x41, 0xf5, 0xe2, 0x4c, 0x28, 0x98, 0x19, 0x66, 0xdb, 0x02, 0x31, 0xac, 0x8e, 0x70,
		0x83, 0x8d, 0x78, 0x9e, 0x1d, 0xe6, 0xff, 0x6c, 0xed, 0xe2, 0xad, 0xf1, 0x24, 0xc8, 0x55, 0xe2,
		0xb7, 0x26, 0x02, 0xfc, 0x19, 0x03, 0xf9, 0x96, 0xe0, 0x16, 0x83, 0xce, 0x8c, 0x5a, 0x88, 0x91,
		0x00, 0x17, 0xbf, 0xf3, 0xf6, 0x3f, 0x40, 0xb6, 0x0a, 0x58, 0x6e, 0x28, 0xf8, 0x8d, 0x15, 0xdc,
		0x87, 0xeb, 0x83, 0x39, 0x06, 0x0c, 0x14, 0xde, 0xcb, 0x96, 0xbf, 0x42, 0xe7, 0xc1, 0x99, 0x81,
		0xe0, 0x82, 0xc9, 0x9d, 0xe3, 0x39, 0x4b, 0x54, 0x22, 0xb8, 0x08, 0x0a, 0x33, 0xd1, 0xf1, 0x4b,
		0xd9, 0x91, 0xe8, 0x7a, 0x64, 0xcd, 0x21, 0x7c, 0x6e, 0xbe, 0xd7, 0xed, 0x8f, 0xc3, 0x22, 0x60,
		0x4e, 0xc6, 0x38, 0xab, 0xb9, 0x3f, 0xc7, 0x42, 0x98, 0x38, 0x19, 0x6d, 0x2e, 0x6d, 0x51, 0x98,
		0x60, 0x66, 0x38, 0x9b, 0x94, 0xc4, 0x9a, 0x6b, 0x0a, 0x63, 0xcc, 0x46, 0xd6, 0x96, 0xff, 0x4a,
		0x4a, 0xef, 0x91, 0x8e, 0xdc, 0xbb, 0x5c, 0xce, 0x53, 0x63, 0xf7, 0x3c, 0xa4, 0x71, 0x0d, 0x6a,
		0xef, 0xce, 0x88, 0x72, 0x94, 0xc0, 0x84, 0x79, 0xf3, 0x79, 0xc8, 0x18, 0x41, 0x31, 0x3e, 0x1b,
		0x6e, 0x50, 0x2a, 0xc6, 0x6e, 0x0b, 0xa8, 0x10, 0x4a, 0x3b, 0x2f, 0x53, 0x57, 0x2f, 0x1f, 0x4b
	);

	argon2d::blake2b::hash(hash, data);
	testAssert(hash == expected);
}

void testArgon2dFillMemory() {
	std::vector<argon2d::Block> cache(Rx_Argon2d_Memory_Blocks);
	argon2d::fillMemory(cache, key);

	using Argon2dBlock_64 = std::array<uint64_t, 128>;
	auto b1 = std::bit_cast<Argon2dBlock_64>(cache[0]);
	auto b2 = std::bit_cast<Argon2dBlock_64>(cache[12253]);
	auto b3 = std::bit_cast<Argon2dBlock_64>(cache[262143]);

	testAssert(b1[0] == 0x191e0e1d23c02186);
	testAssert(b2[29] == 0xf1b62fe6210bf8b1);
	testAssert(b3[127] == 0x1f47f056d05cd99b);

	argon2d::fillMemory(cache, block_template);

	b1 = std::bit_cast<Argon2dBlock_64>(cache[0]);
	b2 = std::bit_cast<Argon2dBlock_64>(cache[12253]);
	b3 = std::bit_cast<Argon2dBlock_64>(cache[262143]);

	testAssert(b1[0] == 0x910af08f94413cfd);
	testAssert(b2[29] == 0x5d4d75503a52283d);
	testAssert(b3[127] == 0x13a957f411409896);
}

void testAesGenerator1RFill() {
	using namespace aes;

	auto state = byte_array(
		0x6c, 0x19, 0x53, 0x6e, 0xb2, 0xde, 0x31, 0xb6, 0xc0, 0x06, 0x5f, 0x7f, 0x11, 0x6e, 0x86, 0xf9,
		0x60, 0xd8, 0xaf, 0x0c, 0x57, 0x21, 0x0a, 0x65, 0x84, 0xc3, 0x23, 0x7b, 0x9d, 0x06, 0x4d, 0xc7,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	);

	auto expected = byte_array(
		0xfa, 0x89, 0x39, 0x7d, 0xd6, 0xca, 0x42, 0x25, 0x13, 0xae, 0xad, 0xba, 0x3f, 0x12, 0x4b, 0x55,
		0x40, 0x32, 0x4c, 0x4a, 0xd4, 0xb6, 0xdb, 0x43, 0x43, 0x94, 0x30, 0x7a, 0x17, 0xc8, 0x33, 0xab,
		0xa3, 0x30, 0x40, 0x6d, 0x94, 0x2c, 0xc6, 0xcd, 0x1d, 0x2b, 0x92, 0xa6, 0x17, 0xb1, 0x72, 0x6c,
		0x56, 0xe2, 0x8c, 0x09, 0x1f, 0x52, 0xd9, 0xd2, 0xeb, 0x2f, 0x52, 0x75, 0x37, 0xf2, 0x75, 0x2a
	);

	std::array<std::byte, 64> actual;
	aes::fill1R(actual, state);

	testAssert(actual == expected);

	auto expected2 = byte_array(
		0x23, 0x11, 0x25, 0xd7, 0x65, 0x43, 0xe8, 0x06, 0xc0, 0x15, 0xcf, 0x2e, 0xdd, 0x46, 0x11, 0xea,
		0xa1, 0x54, 0x95, 0xf8, 0xee, 0xf5, 0xfc, 0x0a, 0x5c, 0x63, 0xab, 0xcc, 0xb3, 0x60, 0x6e, 0x33,
		0xcb, 0x14, 0x90, 0x22, 0xa2, 0xcc, 0x61, 0x88, 0xe4, 0x4e, 0x6b, 0x95, 0xa3, 0xc4, 0x6d, 0x4a,
		0xa9, 0xd5, 0x6a, 0x73, 0x44, 0x66, 0x4d, 0x60, 0x10, 0xb5, 0x5d, 0xaa, 0x60, 0xe2, 0xc2, 0x70,
		0x67, 0xba, 0x55, 0xef, 0xc6, 0xaf, 0x1f, 0x75, 0x89, 0x25, 0xce, 0x70, 0x2d, 0x1b, 0x89, 0x82,
		0xf3, 0x2f, 0x32, 0x22, 0x95, 0x4f, 0x5f, 0x1a, 0xca, 0x3b, 0x42, 0xfd, 0x0f, 0x71, 0x4a, 0xbc,
		0x17, 0xfa, 0x2e, 0x3e, 0x5c, 0x66, 0x82, 0x94, 0x65, 0xbd, 0x54, 0xcd, 0x8c, 0xbe, 0x4d, 0x4c,
		0xc0, 0xb1, 0x64, 0xd9, 0x84, 0x7b, 0xb4, 0x69, 0x66, 0x7b, 0x0c, 0x72, 0xa5, 0xd5, 0xf7, 0x81,
		0x69, 0x44, 0x51, 0xd6, 0x02, 0x94, 0x05, 0xca, 0xd1, 0x2c, 0x7a, 0x82, 0x37, 0x4d, 0xda, 0x2c,
		0xc3, 0xa0, 0x02, 0x18, 0x21, 0x76, 0xbb, 0xf3, 0xef, 0x3d, 0x49, 0x54, 0x06, 0x9e, 0xbb, 0xe5,
		0x81, 0x0a, 0x8d, 0x7f, 0xe9, 0x3e, 0xd5, 0xe7, 0xf2, 0xbc, 0xfb, 0x46, 0x32, 0xa9, 0x57, 0x89,
		0xcb, 0x6c, 0x87, 0x78, 0xfc, 0xa1, 0x4b, 0x73, 0x51, 0x45, 0x97, 0xea, 0xd6, 0xd0, 0x19, 0x86,
		0x86, 0xf3, 0x78, 0x16, 0x33, 0xba, 0x4e, 0x3c, 0x41, 0xed, 0xed, 0xd5, 0xf8, 0x43, 0x9c, 0xb5,
		0xf1, 0x65, 0x6c, 0xb5, 0xc2, 0x4a, 0x93, 0x14, 0xba, 0x8e, 0x08, 0x0b, 0x61, 0x6d, 0x81, 0xf6,
		0xd5, 0x66, 0x35, 0xcd, 0x23, 0x99, 0x54, 0x57, 0x6b, 0x89, 0x69, 0x1a, 0x22, 0x69, 0x5d, 0xa9,
		0x91, 0x3d, 0x05, 0x79, 0xdd, 0x86, 0x08, 0xa3, 0x39, 0xcb, 0x34, 0x1c, 0x67, 0x8e, 0x5f, 0xbe
	);

	std::array<std::byte, 256> actual2;
	aes::fill1R(actual2, state);

	testAssert(actual2 == expected2);
}


void testAesGenerator4RFill() {
	using namespace aes;

	auto state = byte_array(
		0x6c, 0x19, 0x53, 0x6e, 0xb2, 0xde, 0x31, 0xb6, 0xc0, 0x06, 0x5f, 0x7f, 0x11, 0x6e, 0x86, 0xf9,
		0x60, 0xd8, 0xaf, 0x0c, 0x57, 0x21, 0x0a, 0x65, 0x84, 0xc3, 0x23, 0x7b, 0x9d, 0x06, 0x4d, 0xc7,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	);

	auto expected = byte_array(
		0x75, 0x96, 0xe4, 0x22, 0xdb, 0xa5, 0x3f, 0xa5, 0xc1, 0x12, 0x39, 0x11, 0x78, 0x25, 0x68, 0x60,
		0xb4, 0x12, 0x4e, 0x33, 0xc3, 0xc1, 0xa6, 0x28, 0x5f, 0xa0, 0x51, 0xa3, 0xc0, 0xa7, 0x9a, 0xb4,
		0xc9, 0xae, 0x13, 0x20, 0x50, 0x6a, 0xb9, 0x32, 0xd5, 0xad, 0x00, 0xe6, 0x14, 0x5c, 0xd6, 0x58,
		0x55, 0x4d, 0x4c, 0x88, 0x5c, 0xe0, 0x82, 0xb2, 0x30, 0x31, 0xcd, 0x40, 0x71, 0x03, 0xe7, 0x24
	);

	std::array<std::byte, 64> actual;
	aes::fill4R(actual, state);

	testAssert(actual == expected);

	auto expected2 = byte_array(
		0x82, 0x1a, 0xd1, 0x0a, 0x2a, 0x03, 0xeb, 0x20, 0xf2, 0xf3, 0xc2, 0x30, 0x44, 0xd6, 0x3f, 0xb9,
		0xf4, 0x4d, 0x9d, 0xd5, 0x74, 0xde, 0x80, 0x12, 0x5f, 0x6f, 0xd4, 0x81, 0xa4, 0xb9, 0xf9, 0xa4,
		0x3e, 0x6c, 0x6b, 0x6f, 0x03, 0x60, 0x9a, 0xe4, 0x30, 0x7e, 0x84, 0xa3, 0x4c, 0xd0, 0xcd, 0x5d,
		0x13, 0xed, 0xc5, 0xd0, 0x09, 0x62, 0x14, 0x5c, 0xa8, 0xe1, 0xfe, 0x46, 0x52, 0x5b, 0x6f, 0x6e,
		0xa5, 0x75, 0x99, 0x80, 0x3f, 0x7e, 0xd4, 0x61, 0xb4, 0x7c, 0xbb, 0x67, 0x71, 0x40, 0xd9, 0xd2,
		0x58, 0x96, 0x83, 0x58, 0x88, 0x5a, 0xdf, 0xf2, 0x55, 0x19, 0x02, 0x4b, 0x63, 0xc6, 0x72, 0xf7,
		0x32, 0x8a, 0xe8, 0x72, 0xce, 0x28, 0xe8, 0xd6, 0xf0, 0x24, 0xf2, 0xb5, 0xf7, 0xeb, 0x93, 0x1e,
		0x16, 0x25, 0x4a, 0x62, 0x94, 0xfa, 0x77, 0x8c, 0xc6, 0x0d, 0x27, 0xeb, 0x9c, 0x32, 0x04, 0x7d,
		0xfe, 0x78, 0x26, 0xc9, 0x99, 0xc7, 0xb1, 0xbe, 0x41, 0x37, 0xc9, 0xee, 0x08, 0x68, 0x2d, 0x6f,
		0x2a, 0xa9, 0x97, 0x7a, 0x1c, 0x95, 0x39, 0xc1, 0xc4, 0x55, 0x8f, 0x4b, 0xec, 0xf1, 0xe9, 0xa7,
		0xe5, 0x07, 0xa4, 0x12, 0x6b, 0x21, 0x7b, 0xbb, 0x93, 0x47, 0xe0, 0xa5, 0x56, 0xc4, 0xf2, 0x60,
		0x8b, 0x74, 0x4f, 0xd2, 0x0d, 0x7f, 0x44, 0x08, 0x6e, 0xbc, 0x52, 0xa3, 0x87, 0x9f, 0x9f, 0xbe,
		0x0e, 0x56, 0x01, 0x7f, 0x83, 0x1f, 0x12, 0x91, 0x27, 0xcf, 0x15, 0x6f, 0xc6, 0x8c, 0x0c, 0xa6,
		0xbc, 0xad, 0xfd, 0xc1, 0x07, 0x2f, 0xf1, 0x9b, 0xaf, 0xad, 0xe2, 0x06, 0xcb, 0xd0, 0xdc, 0x5d,
		0x99, 0x3b, 0xec, 0xa1, 0x2c, 0xa0, 0xad, 0xa0, 0x35, 0x4e, 0xb2, 0x3a, 0x37, 0x10, 0xa0, 0x43,
		0x64, 0x4e, 0x8b, 0xc1, 0xed, 0x12, 0xc9, 0xc0, 0x15, 0xe1, 0x6a, 0xd2, 0x9a, 0x04, 0xac, 0x78
	);

	std::array<std::byte, 256> actual2;
	aes::fill4R(actual2, state);

	testAssert(actual2 == expected2);
}


void testAesHash1R() {
	auto input = byte_array(
		0x2e, 0x8b, 0xf0, 0x89, 0x47, 0x3a, 0xc5, 0x4d, 0x98, 0x76, 0xc5, 0x53, 0x39, 0x1d, 0xd2, 0x37,
		0xe3, 0x75, 0x79, 0xbd, 0x74, 0x0d, 0x0a, 0xbf, 0x80, 0x73, 0x8f, 0x76, 0x78, 0x05, 0x51, 0xfa,
		0x9d, 0x01, 0x36, 0xbc, 0xfe, 0xf4, 0x39, 0x00, 0x17, 0x90, 0x54, 0x77, 0x12, 0x2d, 0x75, 0xea,
		0x8b, 0xff, 0xf5, 0xa6, 0x41, 0x0e, 0x61, 0x41, 0xdf, 0x1b, 0x12, 0x4c, 0x8d, 0x56, 0x1d, 0xb3
	);

	auto expected = byte_array(
		0x15, 0x6e, 0x43, 0x72, 0x89, 0xbf, 0x89, 0x19, 0xfc, 0x1e, 0x6e, 0x0d, 0xf2, 0x09, 0x93, 0x7a,
		0x58, 0x75, 0xe7, 0x91, 0x2f, 0x76, 0x4e, 0xe9, 0x7f, 0xcf, 0xb4, 0xc8, 0xf4, 0x48, 0xa0, 0x55,
		0xf8, 0xcd, 0xf2, 0xd7, 0xab, 0x41, 0x94, 0x57, 0xe2, 0x62, 0x6b, 0x58, 0x61, 0xfa, 0x6f, 0x83,
		0xc8, 0xf8, 0xc0, 0x6d, 0xd4, 0xac, 0xc1, 0xc3, 0xcd, 0x9b, 0xd0, 0xe3, 0x92, 0xa1, 0xd1, 0x08
	);

	std::array<std::byte, 64> actual{};
	aes::hash1R(actual, input);

	testAssert(actual == expected);


	auto input2 = byte_array(
		0x2e, 0x8b, 0xf0, 0x89, 0x47, 0x3a, 0xc5, 0x4d, 0x98, 0x76, 0xc5, 0x53, 0x39, 0x1d, 0xd2, 0x37,
		0xe3, 0x75, 0x79, 0xbd, 0x74, 0x0d, 0x0a, 0xbf, 0x80, 0x73, 0x8f, 0x76, 0x78, 0x05, 0x51, 0xfa,
		0x9d, 0x01, 0x36, 0xbc, 0xfe, 0xf4, 0x39, 0x00, 0x17, 0x90, 0x54, 0x77, 0x12, 0x2d, 0x75, 0xea,
		0x8b, 0xff, 0xf5, 0xa6, 0x41, 0x0e, 0x61, 0x41, 0xdf, 0x1b, 0x12, 0x4c, 0x8d, 0x56, 0x1d, 0xb3,
		0x2e, 0x8b, 0xf0, 0x89, 0x47, 0x3a, 0xc5, 0x4d, 0x98, 0x76, 0xc5, 0x53, 0x39, 0x1d, 0xd2, 0x37,
		0xe3, 0x75, 0x79, 0xbd, 0x74, 0x0d, 0x0a, 0xbf, 0x80, 0x73, 0x8f, 0x76, 0x78, 0x05, 0x51, 0xfa,
		0x9d, 0x01, 0x36, 0xbc, 0xfe, 0xf4, 0x39, 0x00, 0x17, 0x90, 0x54, 0x77, 0x12, 0x2d, 0x75, 0xea,
		0x8b, 0xff, 0xf5, 0xa6, 0x41, 0x0e, 0x61, 0x41, 0xdf, 0x1b, 0x12, 0x4c, 0x8d, 0x56, 0x1d, 0xb3,
		0x2e, 0x8b, 0xf0, 0x89, 0x47, 0x3a, 0xc5, 0x4d, 0x98, 0x76, 0xc5, 0x53, 0x39, 0x1d, 0xd2, 0x37,
		0xe3, 0x75, 0x79, 0xbd, 0x74, 0x0d, 0x0a, 0xbf, 0x80, 0x73, 0x8f, 0x76, 0x78, 0x05, 0x51, 0xfa,
		0x9d, 0x01, 0x36, 0xbc, 0xfe, 0xf4, 0x39, 0x00, 0x17, 0x90, 0x54, 0x77, 0x12, 0x2d, 0x75, 0xea,
		0x8b, 0xff, 0xf5, 0xa6, 0x41, 0x0e, 0x61, 0x41, 0xdf, 0x1b, 0x12, 0x4c, 0x8d, 0x56, 0x1d, 0xb3,
		0x2e, 0x8b, 0xf0, 0x89, 0x47, 0x3a, 0xc5, 0x4d, 0x98, 0x76, 0xc5, 0x53, 0x39, 0x1d, 0xd2, 0x37,
		0xe3, 0x75, 0x79, 0xbd, 0x74, 0x0d, 0x0a, 0xbf, 0x80, 0x73, 0x8f, 0x76, 0x78, 0x05, 0x51, 0xfa,
		0x9d, 0x01, 0x36, 0xbc, 0xfe, 0xf4, 0x39, 0x00, 0x17, 0x90, 0x54, 0x77, 0x12, 0x2d, 0x75, 0xea,
		0x8b, 0xff, 0xf5, 0xa6, 0x41, 0x0e, 0x61, 0x41, 0xdf, 0x1b, 0x12, 0x4c, 0x8d, 0x56, 0x1d, 0xb3
	);

	auto expected2 = byte_array(
		0x57, 0x68, 0x86, 0xcf, 0x0f, 0x39, 0xf8, 0x2b, 0x6c, 0xb4, 0x04, 0x0f, 0xed, 0x5f, 0x33, 0xfa,
		0xaf, 0x43, 0x5b, 0x5c, 0x49, 0x36, 0x24, 0x54, 0x46, 0x55, 0x79, 0x67, 0x92, 0x15, 0x99, 0xd7,
		0xcc, 0x99, 0xc4, 0xc7, 0xc8, 0x91, 0xa9, 0x84, 0x3a, 0x65, 0xf6, 0x02, 0x8b, 0xcb, 0x41, 0x79,
		0x01, 0x6e, 0x2e, 0x2b, 0xdc, 0x50, 0xf8, 0xbd, 0x6f, 0x29, 0x71, 0xc0, 0x58, 0xe6, 0x14, 0x6e
	);

	aes::hash1R(actual, input2);

	testAssert(actual == expected2);
}

void testBlake2bRandom() {
	blake2b::Random gen{ key, 0 };
	uint32_t expected{ 216 };
	uint32_t actual{ gen.getUint8() };

	testAssert(actual == expected);

	expected = 1645563116;
	actual = gen.getUint32();

	testAssert(actual == expected);

	// Force reseed.
	for (auto i = 0; i < 15; i++) auto _ = gen.getUint32();

	expected = 3927737455;
	actual = gen.getUint32();

	testAssert(actual == expected);
}

void testReciprocal() {
	testAssert(reciprocal(3) == 12297829382473034410U);
	testAssert(reciprocal(13) == 11351842506898185609U);
	testAssert(reciprocal(33) == 17887751829051686415U);
	testAssert(reciprocal(65537) == 18446462603027742720U);
	testAssert(reciprocal(15000001) == 10316166306300415204U);
	testAssert(reciprocal(3845182035) == 10302264209224146340U);
	testAssert(reciprocal(0xffffffff) == 9223372039002259456U);
}

void testSuperscalarGenerate() {
	blake2b::Random gen{ key, 0 };
	Superscalar superscalar{ gen };
	SuperscalarProgram ssProg{ superscalar.generate() };

	// First program.
	testAssert(ssProg.instructions[0].type() == SuperscalarInstructionType::IMUL_R); // First.
	testAssert(ssProg.instructions[215].type() == SuperscalarInstructionType::IADD_C7); // Some in the middle.
	testAssert(ssProg.instructions[446].type() == SuperscalarInstructionType::ISMULH_R); // Last.
	testAssert(ssProg.instructions[447].type() == SuperscalarInstructionType::INVALID); // Following last.
	testAssert(ssProg.address_register == 4);

	// Iterate to last program.
	for (auto i = 1; i < Rx_Cache_Accesses; i++) {
		ssProg = superscalar.generate();
	}

	testAssert(ssProg.instructions[0].type() == SuperscalarInstructionType::IMUL_R); // First.
	testAssert(ssProg.instructions[177].type() == SuperscalarInstructionType::ISMULH_R); // Some in the middle.
	testAssert(ssProg.instructions[436].type() == SuperscalarInstructionType::IMUL_RCP); // Last.
	testAssert(ssProg.instructions[437].type() == SuperscalarInstructionType::INVALID); // Following last.
	testAssert(ssProg.address_register == 0);
}

void testDatasetGenerate() {
	std::vector<argon2d::Block> cache(Rx_Argon2d_Memory_Blocks);
	argon2d::fillMemory(cache, key);
	blake2b::Random blakeRNG{ key, 0 };

	Superscalar superscalar{ blakeRNG };
	std::array<SuperscalarProgram, 8> ssPrograms;
	for (auto i = 0; i < Rx_Cache_Accesses; i++) {
		ssPrograms[i] = superscalar.generate();
		compile(ssPrograms[i]);
	}

	const auto dt{ generateDataset(cache, ssPrograms) };

	testAssert(dt[0][0] == 0x680588a85ae222db);
	testAssert(dt[2][1] == 0xbbe8d699a7c504dc);
	testAssert(dt[3][7] == 0x7908e227a0effb29);
	testAssert(dt[213][7] == 0x81bcac0872ee9d29);
	testAssert(dt[2137213][7] == 0x1dac57c3f3a27a8);
	testAssert(dt[10000000][0] == 0x7943a1f6186ffb72);
	testAssert(dt[20000000][0] == 0x9035244d718095e1);
	testAssert(dt[30000000][0] == 0x145a5091f7853099);
	testAssert(dt[34078719][7] == 0x10844958c957dfc2);
}

void testHasher() {
	auto expected = byte_array(
		0x63, 0x91, 0x83, 0xaa, 0xe1, 0xbf, 0x4c, 0x9a, 0x35, 0x88, 0x4c, 0xb4, 0x6b, 0x09, 0xca, 0xd9,
		0x17, 0x5f, 0x04, 0xef, 0xd7, 0x68, 0x4e, 0x72, 0x62, 0xa0, 0xac, 0x1c, 0x2f, 0x0b, 0x4e, 0x3f
	);

	Hasher hasher(key);
	auto actual{ hasher.run(input) };

	testAssert(actual == expected);

	expected = byte_array(
		0x30, 0x0a, 0x0a, 0xdb, 0x47, 0x60, 0x3d, 0xed, 0xb4, 0x22, 0x28, 0xcc, 0xb2, 0xb2, 0x11, 0x10,
		0x4f, 0x4d, 0xa4, 0x5a, 0xf7, 0x09, 0xcd, 0x75, 0x47, 0xcd, 0x04, 0x9e, 0x94, 0x89, 0xc9, 0x69
	);

	actual = hasher.run(input2);

	testAssert(actual == expected);
}
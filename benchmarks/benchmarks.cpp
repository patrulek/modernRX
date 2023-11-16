#include <atomic>
#include <chrono>
#include <functional>
#include <print>

#include "aes1rhash.hpp"
#include "aes1rrandom.hpp"
#include "aes4rrandom.hpp"
#include "argon2d.hpp"
#include "blake2b.hpp"
#include "dataset.hpp"
#include "hasher.hpp"
#include "superscalar.hpp"

namespace {
    constexpr double Us_Per_Sec{ 1'000'000.0 }; // Number of microseconds in one second.
    constexpr double Total_Microseconds{ 60 * Us_Per_Sec }; // Minimum number of microseconds to run benchmark for. Equal to 60sec.
}

struct BenchmarkResult {
    uint64_t iterations{ 0 }; // Number of iterations performed for the time elapsed.
    double elapsed{ 0.0 }; // Time elapsed in seconds.
    double throughput{ 0.0 }; // Throughput, as number of units processed per second.
};

struct Benchmark {
    std::string name; // Benchmark name.
    uint64_t units; // Units processed by BenchmarkFunction in single opeartion.
    std::string_view unit; // Unit of throughput.

    std::function<void()> function; // BenchmarkFunction to be called.
    BenchmarkResult result; // Benchmark result.
};

void runBenchmarks(std::span<Benchmark> benchmarks) {
    for (auto& bench : benchmarks) {
        constexpr double Bytes_Per_MB{ 1'048'576.0 }; // Number of bytes in one megabyte.
        constexpr std::string_view Fmt_Header{ "{:40s}\n-----\n" };
        constexpr std::string_view Fmt{ "Iterations\tElapsed time\tThroughput\n{:>10d}\t{:>11.3f}s\t{:>6.1f}{:s}\n" };
        double total_elapsed{ 0.0 }; // Total elapsed time in microseconds.

        std::print(Fmt_Header, bench.name);

        // Do single iteration and estimate how many iterations should be performed.
        const auto startB{ std::chrono::high_resolution_clock::now() };
        bench.function();
        const auto stopB{ std::chrono::high_resolution_clock::now() };
        const auto elapsed{ static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(stopB - startB).count()) };

        total_elapsed = elapsed;
        bench.result.iterations = 1;

        while (total_elapsed < Total_Microseconds) {
            uint64_t iterations{ 1 }; // Number of iterations to perform.

            // If duration of single iteration is less than 1 microsecond, assume 1'000'000 of iterations per second.
            if (elapsed < 1) {
                iterations = static_cast<uint64_t>(Total_Microseconds - total_elapsed);
            }
            // Else calculate number of iterations to perform yet. If elapsed is greater than total_seconds, 0 iterations will be performed.
            else {
                iterations = static_cast<uint64_t>((Total_Microseconds - total_elapsed) / elapsed);
            }
            iterations = std::max(iterations, 1ULL);

            // Perform left iterations.
            const auto startB2{ std::chrono::high_resolution_clock::now() };
            for (uint64_t i = 0; i < iterations; ++i) {
                bench.function();
            }
            const auto stopB2{ std::chrono::high_resolution_clock::now() };
            const auto elapsed2{ static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(stopB2 - startB2).count()) };

            total_elapsed += elapsed2;
            bench.result.iterations += iterations;
        }

        const auto total_units{ static_cast<double>(bench.result.iterations * bench.units) };
        const auto elapsed_seconds{ total_elapsed / Us_Per_Sec };

        bench.result.elapsed = total_elapsed;
        bench.result.throughput = total_units / elapsed_seconds;

        if (bench.unit == "B/s") {
            bench.result.throughput /= Bytes_Per_MB;
            bench.unit = "MB/s";
        }

        std::println(Fmt, bench.result.iterations, elapsed_seconds, bench.result.throughput, bench.unit);
    }
}

using namespace modernRX;

void hasherBenchmark();
void blake2bBenchmark();
void blake2bLongBenchmark();
void argon2dFillMemoryBenchmark();
void aes1rHashBenchmark();
void aes1rFillBenchmark();
void aes4rFillBenchmark();
void superscalarGenerateBenchmark();
void datasetGenerateBenchmark();

std::array<std::byte, 64> data;
std::array<std::byte, 72> data_long;
std::array<std::byte, 64> hash;
std::vector<std::byte> hash_long(1024, std::byte(0));
HeapArray<argon2d::Block, 4096> memory(Rx_Argon2d_Memory_Blocks);
std::vector<std::byte> aes_input;
std::vector<std::byte> program_input;
std::array<SuperscalarProgram, Rx_Cache_Accesses> programs;

blake2b::Random gen{ hash, 0 };
Superscalar superscalar{ gen };

std::atomic<uint32_t> nonce{ 0 };
uint32_t fill{ 0 };
uint32_t seed{ 0 };

BlockTemplate block_template{ 
    0x07, 0x07, 0xf7, 0xa4, 0xf0, 0xd6, 0x05, 0xb3, 0x03, 0x26, 0x08, 0x16, 0xba, 0x3f, 0x10, 0x90, 0x2e, 0x1a, 0x14,
    0x5a, 0xc5, 0xfa, 0xd3, 0xaa, 0x3a, 0xf6, 0xea, 0x44, 0xc1, 0x18, 0x69, 0xdc, 0x4f, 0x85, 0x3f, 0x00, 0x2b, 0x2e,
    0xea, 0x00, 0x00, 0x00, 0x00, 0x77, 0xb2, 0x06, 0xa0, 0x2c, 0xa5, 0xb1, 0xd4, 0xce, 0x6b, 0xbf, 0xdf, 0x0a, 0xca,
    0xc3, 0x8b, 0xde, 0xd3, 0x4d, 0x2d, 0xcd, 0xee, 0xf9, 0x5c, 0xd2, 0x0c, 0xef, 0xc1, 0x2f, 0x61, 0xd5, 0x61, 0x09
};

int main() {
    aes_input.resize(Rx_Scratchpad_L3_Size);
    program_input.resize(2176);

    for (auto &program : programs) {
        program = superscalar.generate();
    }

    std::vector<Benchmark> benchmarks{
        { "Blake2b::hash (64B input/output)", 1, "H/s", blake2bBenchmark },
        { "Argon2d::Blake2b::hash (72B input, 1 KB output)", 1, "H/s", blake2bLongBenchmark },
        { "Argon2d::fillMemory (256MB output)", 268'435'456, "B/s", argon2dFillMemoryBenchmark },
        { "Aes::fill1R (64B input, 2MB output)",  2'097'152, "B/s", aes1rFillBenchmark },
        { "Aes::fill4R (64B input, 2176B output)", 2176, "B/s", aes4rFillBenchmark },
        { "Aes::hash1R (2MB input, 64B output)", 1, "H/s", aes1rHashBenchmark },
        { "Superscalar::generate (1 Program output)", 1, "Program/s", superscalarGenerateBenchmark },
        { std::format("Dataset::generate ({:d}B output)", Rx_Dataset_Base_Size + Rx_Dataset_Extra_Size), Rx_Dataset_Base_Size + Rx_Dataset_Extra_Size, "B/s", datasetGenerateBenchmark },
    };

    std::println("Running {:d} benchmarks...\n", benchmarks.size());
    runBenchmarks(benchmarks);

    hasherBenchmark();
}

void hasherBenchmark() {
    try {
        std::println("Running Hasher benchmark...");

        Hasher hasher{ span_cast<std::byte>(seed) };
        hasher.resetVM(block_template);
        std::atomic<uint64_t> counter{ 0 };

        const auto startT{ std::chrono::high_resolution_clock::now() };
        hasher.run([&counter](const RxHash& hash) {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
        std::this_thread::sleep_for(std::chrono::microseconds(static_cast<uint64_t>(Total_Microseconds)));
        hasher.stop();
        const auto endT{ std::chrono::high_resolution_clock::now() };

        const auto elapsedT{ static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(endT - startT).count()) };
        const auto throughputT{ static_cast<double>(counter.load()) / (elapsedT / Us_Per_Sec) };

        std::println("Benchmark ran for {:.3f}s", elapsedT / Us_Per_Sec);
        std::println("Hashes calculated: {:d}", counter.load());
        std::println("Hashes per second: {:.2f}", throughputT);
    } catch (const Exception& ex) {
        std::println("Failed to initialize Hasher: {:s}", ex.what());
        return std::exit(-1);
    }
}

void blake2bBenchmark() {
    blake2b::hash(hash, data);
}

void blake2bLongBenchmark() {
    argon2d::blake2b::hash(hash_long, data_long);
}

void argon2dFillMemoryBenchmark() {
    auto block_template_copy{ block_template };
    std::memcpy(block_template_copy.data + 11, &(++fill), sizeof(uint32_t));
    argon2d::fillMemory(memory.buffer(), block_template.view());
}

void aes1rHashBenchmark() {
    aes::hash1R(hash, aes_input);
}

void aes1rFillBenchmark() {
    aes::fill1R(aes_input, hash);
}

void aes4rFillBenchmark() {
    aes::fill4R(program_input, hash);
}

void superscalarGenerateBenchmark() {
    auto _ { superscalar.generate() };
}

void datasetGenerateBenchmark() {
    for (auto& program : programs) {
        program = superscalar.generate();
    }

    auto _ { generateDataset(memory.view(), programs)};
}

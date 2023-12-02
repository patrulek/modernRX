#include <atomic>
#include <chrono>
#include <functional>
#include <print>

#include "argon2d.hpp"
#include "blake2b.hpp"
#include "dataset.hpp"
#include "hasher.hpp"
#include "superscalar.hpp"
#include "trace.hpp"

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

struct Options {
    int seconds{ 60 };
    int verbose{ 1 };
    int warmup{ 5 };
    bool microbenchmarks{ true };

    std::string_view usage() const {
        return "benchmarks [--warmup <seconds:0-15, default: 5>] [--seconds <seconds:15-7200, default: 60>] [--verbose <level:0-2, default: 1>] [--no-microbenchmarks]";
    }

    void parse(int argc, char** argv) {
        std::string_view arg;
        int* iarg{ nullptr };
        int min_range{ 0 };
        int max_range{ 0 };

        for (int i = 1; i < argc; ++i) {
            if (iarg != nullptr) {
                *iarg = std::min(std::max(std::stoi(argv[i]), min_range), max_range);
                iarg = nullptr;
                continue;
            }

            arg = argv[i];
            if (arg == "--seconds") {
                iarg = &seconds;
                min_range = 15; max_range = 7200;
            } else if (arg == "--verbose") {
                iarg = &verbose;
                min_range = 0; max_range = 2;
            } else if (arg == "--no-microbenchmarks") {
                microbenchmarks = false;
            } else if (arg == "--warmup") {
                iarg = &warmup;
                min_range = 0; max_range = 15;
            } else {
                throw std::runtime_error{ std::format("invalid argument `{}`", arg) };
            }
        }

        if (iarg != nullptr) {
            throw std::runtime_error{ std::format("missing argument value for `{}`", arg) };
        }
    }
};

void hasherBenchmark(const Options options);
void microbenchmarks();
void blake2bBenchmark();
void blake2bLongBenchmark();
void argon2dFillMemoryBenchmark();
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

int main(int argc, char **argv) {
    Options options;

    if (argc > 1) {
        try {
            options.parse(argc, argv);
        } catch (const std::exception& ex) {
            std::println("Failed to parse arguments: {:s}", ex.what());
            std::println("Usage: {}", options.usage());
            std::exit(-1);
        } catch (...) {
            std::println("Failed to parse arguments.");
            std::println("Usage: {}", options.usage());
            std::exit(-1);
        }
    }

    if (options.microbenchmarks) {
        microbenchmarks();
    }

    hasherBenchmark(options);
}

void hasherBenchmark(const Options options) {
    TraceResults trace_results;

    try {
        std::println("Running Hasher benchmark with options:\n- seconds: {:d}\n- warmup: {:d}\n- verbosity: {:d}\n- trace: {}\n", 
            options.seconds, options.warmup, options.verbose, Trace_Enabled);

        std::println("Key: {:08x}", seed);
        std::print("Block template: ");
        for (const auto& b : block_template.view()) {
            std::print("{:02x}", static_cast<uint8_t>(b));
        }
        std::println("");

        auto startT{ std::chrono::high_resolution_clock::now() };
        Hasher hasher{ span_cast<std::byte>(seed) };
        auto endT{ std::chrono::high_resolution_clock::now() };
        auto elapsedT{ static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(endT - startT).count()) };
        std::println("Memory initialized in {:.3f}s\n", elapsedT / Us_Per_Sec);

        hasher.resetVM(block_template);

        std::vector<std::pair<size_t, size_t>> measures;
        measures.reserve(options.seconds + 1);
        measures.emplace_back(0, 0);

        startT = std::chrono::high_resolution_clock::now();
        hasher.run();
        while (measures.size() < options.seconds + 1) {
            std::this_thread::sleep_for(std::chrono::microseconds(static_cast<uint64_t>(Us_Per_Sec)));
            const auto hashes = hasher.hashes();
            const auto tickT{ std::chrono::high_resolution_clock::now() };
            const auto elapsedT{ static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(tickT - startT).count()) };
            measures.emplace_back(hashes, static_cast<size_t>(elapsedT));
        }

        hasher.stop();
        endT = std::chrono::high_resolution_clock::now();

        const size_t warmup_hashes{ measures[options.warmup].first };
        elapsedT = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(endT - startT).count() - measures[options.warmup].second);
        const auto throughputT{ static_cast<double>(hasher.hashes() - warmup_hashes) / (elapsedT / Us_Per_Sec) };


        if (options.verbose >= 2) {
            std::println("(initial) Hashes: 0\tTimestamp: 0.000s\tH: 0\tT: 0.000s\tAvg h/s: 0.000");
        }

        double variance{ 0.0 };
        std::vector<double> hs_vec;

        for (int i = 1; i < measures.size(); ++i) {
            const auto& cur{ measures[i] };
            const auto& prev{ measures[i - 1] };
            
            const double h{ static_cast<double>(cur.first - prev.first) };
            const double t{ (cur.second - prev.second) / Us_Per_Sec };
            const bool is_warmup{ i <= options.warmup };

            if (options.verbose >= 2) {
                const std::string_view warmup_str{ (is_warmup ? "(warmup) " : "") };
                std::println("{}Hashes: {:d}\tTimestamp: {:.3f}s\tH: {}\tT: {:.3f}s\tAvg h/s: {:.3f}", 
                    warmup_str, cur.first, cur.second / Us_Per_Sec, h, t, h / t);
            }

            if (!is_warmup) {
                hs_vec.push_back(h / t);
                variance += (h / t - throughputT) * (h / t - throughputT);
            }
        }

        if (options.verbose >= 2) {
            std::println("");
        }

        std::sort(hs_vec.begin(), hs_vec.end());
        variance /= hs_vec.size();
        const auto stddev{ std::sqrt(variance) };
        const auto p5_idx{ static_cast<size_t>(hs_vec.size() * (1.0 - 0.05)) };
        const auto p50_idx{ static_cast<size_t>(hs_vec.size() * (1.0 - 0.50)) };
        const auto p95_idx{ static_cast<size_t>(hs_vec.size() * (1.0 - 0.95)) };

        if (options.verbose >= 1) {
            std::println("min h/s: {:.3f}", hs_vec.front());
            std::println("95th percentile: {:.3f}", hs_vec[p95_idx]);
            std::println("50th percentile: {:.3f}", hs_vec[p50_idx]);
            std::println("5th percentile: {:.3f}", hs_vec[p5_idx]);
            std::println("max h/s: {:.3f}", hs_vec.back());
            std::println("");
        }

        const std::string_view warmup_time_str{ (options.warmup == 0 ? "" : std::format(" (+{:.3f}s warmup)", measures[options.warmup].second / Us_Per_Sec)) };
        std::println("Benchmark ran for {:.3f}s{}", elapsedT / Us_Per_Sec, warmup_time_str);

        const std::string_view warmup_hashes_str{ (options.warmup == 0 ? "" : std::format(" (+{:d} warmup hashes)", warmup_hashes)) };
        std::println("Hashes calculated: {:d}{}", hasher.hashes() - warmup_hashes, warmup_hashes_str);
        std::println("Hashes per second: {:.2f}, stddev: {:.2f}", throughputT, stddev);
    } catch (const Exception& ex) {
        std::println("Failed to initialize Hasher: {:s}", ex.what());
        return std::exit(-1);
    }
}

void microbenchmarks() {
    for (auto& program : programs) {
        program = superscalar.generate();
    }

    std::vector<Benchmark> benchmarks{
        { "Blake2b::hash (64B input/output)", 1, "H/s", blake2bBenchmark },
        { "Argon2d::Blake2b::hash (72B input, 1 KB output)", 1, "H/s", blake2bLongBenchmark },
        { "Argon2d::fillMemory (256MB output)", 268'435'456, "B/s", argon2dFillMemoryBenchmark },
        { "Superscalar::generate (1 Program output)", 1, "Program/s", superscalarGenerateBenchmark },
        { std::format("Dataset::generate ({:d}B output)", Rx_Dataset_Base_Size + Rx_Dataset_Extra_Size), Rx_Dataset_Base_Size + Rx_Dataset_Extra_Size, "B/s", datasetGenerateBenchmark },
    };

    std::println("Running {:d} benchmarks...\n", benchmarks.size());
    runBenchmarks(benchmarks);
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

void superscalarGenerateBenchmark() {
    auto _ { superscalar.generate() };
}

void datasetGenerateBenchmark() {
    for (auto& program : programs) {
        program = superscalar.generate();
    }

    auto _ { generateDataset(memory.view(), programs)};
}

#include <iterator>

#include "argon2d.hpp"
#include "cpuinfo.hpp"
#include "exception.hpp"
#include "hasher.hpp"
#include "jitcompiler.hpp"
#include "randomxparams.hpp"
#include "scratchpad.hpp"
#include "superscalar.hpp"


namespace modernRX {
    Hasher::Hasher() {
        checkCPU();
    }

    Hasher::Hasher(const_span<std::byte> key) :
        Hasher() {
        reset(key);
    }

    void Hasher::checkCPU() const {
        if (!CPUInfo::AVX2()) {
            throw Exception{ "AVX2 instructions required but not supported on current CPU" };
        }

        if (!CPUInfo::AES()) {
            throw Exception{ "AES instructions required but not supported on current CPU" };
        }
    }

    void Hasher::reset(const_span<std::byte> key) {
        constexpr uint32_t Programs_Count{ Rx_Cache_Accesses }; // Number of superscalar programs should be equal to number of cache accesses.

        if (!this->key.empty() && std::equal(key.begin(), key.end(), this->key.begin())) {
            return;
        }

        this->key.clear();
        this->key.reserve(key.size());
        std::copy(key.begin(), key.end(), std::back_inserter(this->key));

        HeapArray<argon2d::Block, 4096> cache(Rx_Argon2d_Memory_Blocks);
        argon2d::fillMemory(cache.buffer(), key);

        blake2b::Random blakeRNG{ key, 0 };
        Superscalar superscalar{ blakeRNG };

        std::array<SuperscalarProgram, Programs_Count> programs;
        for (auto& program : programs) {
            program = superscalar.generate();
        }

        dataset = generateDataset(cache.view(), programs);
    }

    std::array<std::byte, 32> Hasher::run(const_span<std::byte> input) {
        std::array<std::byte, 64> seed{};
        blake2b::hash(seed, input);

        Interpreter interpreter{ seed, dataset.view() };
        return interpreter.execute();
    }
}

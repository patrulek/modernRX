#include <iterator>

#include "argon2d.hpp"
#include "hasher.hpp"
#include "jitcompiler.hpp"
#include "randomxparams.hpp"
#include "scratchpad.hpp"
#include "superscalar.hpp"


namespace modernRX {
    Hasher::Hasher(const_span<std::byte> key) {
        reset(key);
    }

    void Hasher::reset(const_span<std::byte> key) {
        constexpr uint32_t programs_count{ Rx_Cache_Accesses }; // Number of superscalar programs should be equal to number of cache accesses.

        if (!this->key.empty() && std::equal(key.begin(), key.end(), this->key.begin())) {
            return;
        }

        this->key.reserve(key.size());
        std::copy(key.begin(), key.end(), std::back_inserter(this->key));

        HeapArray<argon2d::Block, 4096> cache(Rx_Argon2d_Memory_Blocks);
        argon2d::fillMemory(cache.buffer(), key);

        blake2b::Random blakeRNG{ key, 0 };
        Superscalar superscalar{ blakeRNG };

        std::array<SuperscalarProgram, programs_count> programs;
        for (auto& program : programs) {
            program = superscalar.generate();
            compile(program);
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
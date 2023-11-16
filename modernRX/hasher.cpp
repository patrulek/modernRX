#include <algorithm>
#include <iterator>

#include "argon2d.hpp"
#include "cpuinfo.hpp"
#include "datasetcompiler.hpp"
#include "exception.hpp"
#include "hasher.hpp"
#include "randomxparams.hpp"
#include "superscalar.hpp"


namespace modernRX {
    Hasher::Hasher() {
        checkCPU();
    }

    Hasher::Hasher(const_span<std::byte> key) :
        Hasher() {
        // If hyper-threading is enabled, use half of available threads.
        // Possibly it would be better to use L3 cache size to calculate number of threads.
        const auto threads{ CPUInfo::HTT() ? std::thread::hardware_concurrency() / 2 : std::thread::hardware_concurrency() };
        vms.resize(threads);
        vm_workers.reserve(vms.size());
        reset(key);
    }

    Hasher::~Hasher() {
        stop();
    }

    void Hasher::run(std::function<void(const RxHash&)> callback) {
        bool expected{ false };
        if (!vm_workers.empty() || !running.compare_exchange_strong(expected, true)) {
            // Already running.
            return;
        }

        // If callback is not provided, use empty function.
        if (callback == nullptr) {
            callback = [](const RxHash&) {};
        }

        for (auto& vm : vms) {
            vm_workers.emplace_back([&vm, callback, this]() {
                for (; this->running; ) {
                    vm.execute(callback);
                }
            });
        }
    }

    void Hasher::stop() {
        bool expected{ true };
        if (vm_workers.size() < vms.size() || !running.compare_exchange_strong(expected, false)) {
            // Not running or stopping.
            return;
        }

        for (auto& worker : vm_workers) {
            worker.join();
        }
        vm_workers.clear();
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

    void Hasher::resetVM(BlockTemplate block_template) {
        const uint32_t offset{ static_cast<uint32_t>(std::numeric_limits<uint32_t>::max() / vms.size()) };

        for (auto& vm : vms) {
            vm.reset(block_template, dataset.view());
            block_template.next(offset);
        }
    }
}

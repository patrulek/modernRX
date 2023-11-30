#include <algorithm>
#include <iterator>

#include "argon2d.hpp"
#include "cpuinfo.hpp"
#include "datasetcompiler.hpp"
#include "exception.hpp"
#include "hasher.hpp"
#include "randomxparams.hpp"
#include "superscalar.hpp"
#include "thread.hpp"

namespace modernRX {
    Hasher::Hasher() {
        checkCPU();

        // If hyper-threading is enabled, use half of available threads.
        // Possibly it would be better to use L3 cache size to calculate number of threads.
        const auto threads{ CPUInfo::HTT() ? std::thread::hardware_concurrency() / 2 : std::thread::hardware_concurrency() };
        vms.reserve(threads);
        vm_workers.reserve(vms.size());

        // Allocate memory for VMs.
        constexpr auto Vm_Required_Memory{ VirtualMachine::requiredMemory() };
        constexpr auto Offset{ 0 };
        constexpr auto Total_Vm_Memory{ Vm_Required_Memory + Offset };
        scratchpads.reserve(threads * Total_Vm_Memory);

        for (uint32_t i = 0; i < threads; ++i) {
            vms.emplace_back(scratchpads.buffer<Vm_Required_Memory>(i * Total_Vm_Memory, Vm_Required_Memory), i);
        }
    }

    Hasher::Hasher(const_span<std::byte> key) :
        Hasher() {

        reset(key);
    }

    Hasher::~Hasher() {
        stop();
    }

    uint64_t Hasher::hashes() const noexcept {
        uint64_t hashes{ 0 };
        for (const auto& vm : vms) {
            hashes += vm.getPData().hashes;
        }

        return hashes;
    }

    void Hasher::run(std::function<void(const RxHash&)> callback) {
        bool expected{ false };
        if (!vm_workers.empty() || !running.compare_exchange_strong(expected, true)) {
            // Already running.
            return;
        }

        std::atomic<size_t> vm_init{ 0 };

        for (uint32_t vm_id = 0; vm_id < vms.size(); ++vm_id) {
            vm_workers.emplace_back([&vm_init, callback, vm_id, this]() {
                vm_init.fetch_add(1, std::memory_order_relaxed);

                if (!setThreadAffinity(vm_id)) {
                    vm_init.fetch_add(vms.size(), std::memory_order_relaxed);
                    return;
                }

                auto& vm{ vms[vm_id] };
                while (this->running) {
                    vm.execute(callback);
                }
            });
        }

        // Wait for worker threads to initialize.
        while (vm_init.load(std::memory_order_relaxed) < vms.size()) {
            std::this_thread::yield();
        }

        // If any thread failed to initialize, stop and throw exception.
        if (vm_init.load(std::memory_order_relaxed) > vms.size()) {   
            stop();
            throw modernRX::Exception("Failed to initialize VirtualMachine worker threads");
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

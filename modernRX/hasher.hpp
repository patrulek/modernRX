#pragma once

/*
* Multi-threaded RandomX hash generator.
*/

#include <atomic>
#include <vector>

#include "dataset.hpp"
#include "heaparray.hpp"
#include "virtualmachine.hpp"

namespace modernRX {
    class Hasher {
    public:
        // Initialize with empty key (for later reset).
        [[nodiscard]] explicit Hasher();
        
        // Initialize with key to generate Dataset at creation
        [[nodiscard]] explicit Hasher(const_span<std::byte> key);

        ~Hasher();

        Hasher(const Hasher&) = delete;
        Hasher& operator=(const Hasher&) = delete;
        Hasher(Hasher&&) = delete;
        Hasher& operator=(Hasher&&) = delete;

        // Resets VirtualMachine's states with given block template.
        void resetVM(BlockTemplate block);

        // Resets Dataset with new key. Does nothing if key is equal to previous one.
        void reset(const_span<std::byte> key);

        // Starts all VirtualMachine worker threads.
        void run(std::function<void(const RxHash&)> callback = [](const RxHash&) noexcept {});

        // Wait for all VirtualMachine worker threads to finish.
        void stop();
    private:
        std::vector<std::thread> vm_workers; // Threads used for program execution.
        std::vector<VirtualMachine> vms; // Virtual machines used for program execution.
        std::vector<std::byte> key; // Latest key used for Dataset generation.
        HeapArray<DatasetItem, 4096> dataset; // Dataset used for program execution.
        HeapArray<std::byte, 64 * Rx_Scratchpad_L3_Size> scratchpads; // Scratchpads used for program execution.
        std::atomic<bool> running{ false }; // Stop signal for VM threads.

        void checkCPU() const; // Ensure CPU supports required features.
    };
}

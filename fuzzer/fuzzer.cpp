#include <print>
#include <thread>

#include "3rdparty/RandomX/src/dataset.hpp"
#include "3rdparty/RandomX/src/jit_compiler.hpp"

#include "virtualmachine.hpp"

using namespace modernRX;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    constexpr size_t Max_Data_Size{ Rx_Block_Template_Size };
    std::array<std::byte, Max_Data_Size> key_or_data;

    for (size_t i = 0; i + size <= Max_Data_Size && size != 0; i += size) {
        std::memcpy(key_or_data.data() + i, data, size);
    }

    try {
        // Initialize modernRX virtual machine
        HeapArray<argon2d::Block, 4096> cache(Rx_Argon2d_Memory_Blocks);
        argon2d::fillMemory(cache.buffer(), key_or_data);

        blake2b::Random blakeRNG{ key_or_data, 0 };
        Superscalar superscalar{ blakeRNG };

        std::array<SuperscalarProgram, Rx_Cache_Accesses> programs;
        for (auto& program : programs) {
            program = superscalar.generate();
        }

        auto dataset{ generateDataset(cache.view(), programs) };
        HeapArray<std::byte, Rx_Scratchpad_L3_Size> scratchpad(VirtualMachine::requiredMemory());
        auto vm_scratchpad = scratchpad.buffer<VirtualMachine::requiredMemory()>();
        auto vm_jit_buffer = makeExecutable<JITRxProgram>(12 * 1024);

        VirtualMachine vm(vm_scratchpad, reinterpret_cast<JITRxProgram>(vm_jit_buffer.get()));
        BlockTemplate bt;
        std::memcpy(bt.data, key_or_data.data(), key_or_data.size());

        // Initialize RandomX dataset and VM
        auto initThreadCount{ std::thread::hardware_concurrency() };
        auto rx_flags{ randomx_get_flags() | RANDOMX_FLAG_FULL_MEM };
        auto rx_cache{ randomx_alloc_cache(rx_flags) };
        randomx_init_cache(rx_cache, key_or_data.data(), key_or_data.size());

        auto rx_dataset{ randomx_alloc_dataset(rx_flags) };
        auto datasetItemCount{ randomx_dataset_item_count() };
        auto perThread{ datasetItemCount / initThreadCount };
        auto remainder{ datasetItemCount % initThreadCount };
        auto startItem{ 0 };
        std::vector<std::thread> threads;

        for (int i = 0; i < initThreadCount; ++i) {
            auto count{ perThread + (i == initThreadCount - 1 ? remainder : 0) };
            threads.push_back(std::thread(&randomx_init_dataset, rx_dataset, rx_cache, startItem, count));
            startItem += count;
        }

        for (unsigned i = 0; i < threads.size(); ++i) {
            threads[i].join();
        }

        randomx_vm* rx_vm = randomx_create_vm(rx_flags, rx_cache, rx_dataset);
        std::array<uint8_t, RANDOMX_HASH_SIZE> randomx_hash{};

        // Calculate hashes for modernRX and RandomX to compare.
        for (int i = 0; i < size; ++i) {
            RxHash modernRX_hash;
            vm.reset(bt, dataset);
            vm.execute(nullptr); // First 'execute' after reset does only initialization.
            vm.execute([&modernRX_hash](const RxHash& hash) {
                modernRX_hash = hash;
            });
            randomx_calculate_hash(rx_vm, key_or_data.data(), key_or_data.size(), randomx_hash.data());

            if (!std::equal(modernRX_hash.data.begin(), modernRX_hash.data.end(), randomx_hash.begin())) {
                std::abort();
            }

            key_or_data[(i + size) % Max_Data_Size] = static_cast<std::byte>(static_cast<uint8_t>(key_or_data[(i + size) % Max_Data_Size]) + 1);
            std::memcpy(bt.data, key_or_data.data(), key_or_data.size());
        }

        randomx_release_cache(rx_cache);
        randomx_release_dataset(rx_dataset);
        randomx_destroy_vm(rx_vm);
    } catch (const std::exception& e) {
        std::println("Unexpected exception: {}", e.what());
        std::abort();
    }

    return 0;
}

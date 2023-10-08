#include <thread>

#include "argon2d.hpp"
#include "assertume.hpp"
#include "dataset.hpp"
#include "superscalar.hpp"

namespace modernRX {
    namespace {

        constexpr uint32_t cache_item_count{ argon2d::Memory_Size / sizeof(DatasetItem) };
        static_assert(std::has_single_bit(cache_item_count)); // Vectorized code assumes that cache_item_count is a power of 2.
    }

    HeapArray<DatasetItem, 4096> generateDataset(const_span<argon2d::Block> cache, const_span<SuperscalarProgram, Rx_Cache_Accesses> programs) {
        // It can be assumed that cache size is static and equal to Rx_Argon2d_Memory_Blocks.
        ASSERTUME(cache.size() == Rx_Argon2d_Memory_Blocks);

        auto jit{ compile(programs) };

        // Allocate memory for dataset.
        const uint32_t thread_count{ std::thread::hardware_concurrency() };

        // Dataset padding size adds additional memory to dataset to make it divisible by thread count * batch_size(4) without remainder.
        // This is needed to make sure that each thread will have the same amount of work and no additional function for handling remainders is needed.
        // Additional data will be ignored during hash calculation, its purpose is to simplify dataset generation.
        const uint32_t dataset_alignment{ thread_count * 4 * sizeof(DatasetItem) };
        const uint32_t dataset_padding_size{ dataset_alignment - ((Rx_Dataset_Base_Size + Rx_Dataset_Extra_Size) % dataset_alignment) };
        const uint32_t Dataset_Items_Count{ (Rx_Dataset_Base_Size + Rx_Dataset_Extra_Size + dataset_padding_size) / sizeof(DatasetItem) };
        HeapArray<DatasetItem, 4096> memory(Dataset_Items_Count);

        const uint32_t items_per_thread{ Dataset_Items_Count / thread_count }; // Number of items per additional thread.

        // Assume that number of items per thread is always greater than 0. If not, invalid or exotic configuration is used.
        ASSERTUME(items_per_thread > 0);

        // Task that will be executed by each thread.
        const auto task = [jit_program = *jit, cache_ptr = reinterpret_cast<uintptr_t>(cache.data())](std::span<DatasetItem> submemory, const uint32_t tid) {
            constexpr uint32_t cache_item_mask{ cache_item_count - 1 };
            const auto start_item{ tid * submemory.size() };
            jit_program(submemory, cache_ptr, cache_item_mask, start_item);
        };

        // Start threads.
        std::vector<std::thread> threads(thread_count);

        for (uint32_t tid = 0; tid < thread_count; ++tid) {
            threads[tid] = std::thread{ task, memory.buffer(tid * items_per_thread, items_per_thread), tid };
        }

        // Wait for threads to finish.
        for (uint64_t tid = 0; tid < thread_count; ++tid) {
            threads[tid].join();
        }

        return memory;
    }
}
#include <thread>

#include "argon2d.hpp"
#include "assertume.hpp"
#include "dataset.hpp"
#include "superscalar.hpp"

namespace modernRX {
    namespace {
        constexpr uint32_t cache_item_count{ argon2d::Memory_Size / sizeof(DatasetItem) }; // Number of dataset items contained in cache.
        static_assert(std::has_single_bit(cache_item_count)); // Vectorized code assumes that cache_item_count is a power of 2.

        constexpr uint32_t cache_item_mask{ cache_item_count - 1 }; // Mask used to get cache item for dataset item calculation.
    }

    HeapArray<DatasetItem, 4096> generateDataset(const_span<argon2d::Block> cache, const_span<SuperscalarProgram, Rx_Cache_Accesses> programs) {
        // Compile superscalar programs into single function.
        const auto jit{ compile(programs) };

        // Dataset padding size adds additional memory to dataset to make it divisible by thread count * batch_size(4) without remainder.
        // This is needed to make sure that each thread will have the same amount of work and no additional function for handling remainders is needed.
        // Additional data will be ignored during hash calculation, its purpose is to simplify dataset generation.
        const uint32_t thread_count{ std::thread::hardware_concurrency() };
        const uint32_t dataset_alignment{ thread_count * 4 * sizeof(DatasetItem) };
        const uint32_t dataset_padding_size{ dataset_alignment - ((Rx_Dataset_Base_Size + Rx_Dataset_Extra_Size) % dataset_alignment) };
        const uint32_t dataset_items_count{ (Rx_Dataset_Base_Size + Rx_Dataset_Extra_Size + dataset_padding_size) / sizeof(DatasetItem) };
        const uint32_t items_per_thread{ dataset_items_count / thread_count };

        // Allocate memory for dataset.
        HeapArray<DatasetItem, 4096> memory(dataset_items_count);

        // Split each thread task into smaller jobs. This is for reducing potential variances in execution.
        constexpr uint32_t Min_Items_Per_Job{ 32'768 }; // Value was chosen empirically.
        uint32_t task_divisor{ 1 };
        uint32_t new_dataset_padding_size = dataset_padding_size;
        while (items_per_thread / task_divisor > Min_Items_Per_Job && new_dataset_padding_size == dataset_padding_size) {
            task_divisor *= 2;
            const uint32_t new_dataset_alignment{ dataset_alignment * task_divisor };
            new_dataset_padding_size = new_dataset_alignment - ((Rx_Dataset_Base_Size + Rx_Dataset_Extra_Size) % new_dataset_alignment);
        }
        task_divisor = std::max<uint32_t>(1, task_divisor / 2); // Prevents task_divisor from being 0.

        const uint32_t items_per_job{ items_per_thread / task_divisor };
        const uint32_t max_jobs{ dataset_items_count / items_per_job };
        std::atomic<uint32_t> job_counter{ 0 };

        // Task that will be executed by each thread.
        const auto task = [max_jobs, items_per_job, &job_counter, jit_program{ *jit }, cache_ptr{ cache.data() }](std::span<DatasetItem> memory) {
            auto job_id = job_counter.fetch_add(1, std::memory_order_relaxed);
            while (job_id < max_jobs) {
                const auto start_item{ job_id * items_per_job };
                jit_program(memory.subspan(start_item, items_per_job), reinterpret_cast<uintptr_t>(cache_ptr), cache_item_mask, start_item);
                job_id = job_counter.fetch_add(1, std::memory_order_relaxed);
            }
        };

        // Start threads.
        std::vector<std::thread> threads(thread_count);

        for (uint32_t tid = 0; tid < thread_count; ++tid) {
            threads[tid] = std::thread{ task, memory.buffer() };
        }

        // Wait for threads to finish.
        for (uint64_t tid = 0; tid < thread_count; ++tid) {
            threads[tid].join();
        }

        return memory;
    }
}
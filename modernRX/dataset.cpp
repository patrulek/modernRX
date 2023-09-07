#include <thread>

#include "argon2d.hpp"
#include "assertume.hpp"
#include "dataset.hpp"
#include "logger.hpp"
#include "superscalar.hpp"


namespace modernRX {
    namespace {
        [[nodiscard]] DatasetItem generateItem(const argon2d::Memory& cache, const_span<Program, Rx_Cache_Accesses> programs, const uint64_t item_number) noexcept;
    }

    std::vector<DatasetItem> generateDataset(const argon2d::Memory& cache, const_span<Program, Rx_Cache_Accesses> programs) {
        // It can be assumed that cache size is static and equal to Rx_Argon2d_Memory_Blocks.
        ASSERTUME(cache.size() == Rx_Argon2d_Memory_Blocks);

        // Allocate memory for dataset.
        constexpr uint64_t Dataset_Items_Count{ (Rx_Dataset_Base_Size + Rx_Dataset_Extra_Size) / sizeof(DatasetItem) };
        std::vector<DatasetItem> memory(Dataset_Items_Count);

        const uint64_t thread_count{ std::thread::hardware_concurrency() };
        const uint64_t items_per_thread{ Dataset_Items_Count / thread_count }; // Number of items per additional thread.
        const uint64_t items_first_thread{ items_per_thread + Dataset_Items_Count % thread_count }; // First thread will possibly have more items to process to make sure no item is left behind.
        
        // Task that will be executed by each thread.
        const auto task = [&cache, &programs, items_first_thread](const uint64_t tid, std::span<DatasetItem> submemory) {
            const uint64_t start_item{ items_first_thread + (tid - 1) * submemory.size() };
            const uint64_t end_item{ start_item + submemory.size() };

            for (size_t i = start_item, j = 0; i < end_item; ++i, ++j) {
                submemory[j] = generateItem(cache, programs, i);
            }
        };

        // Start threads.
        std::vector<std::thread> threads(thread_count);

        for (uint64_t tid = 1; tid < thread_count; ++tid) {
            threads[tid] = std::thread{ task, tid, std::span<DatasetItem>{ memory.begin() + items_first_thread + (tid - 1) * items_per_thread, items_per_thread} };
        }

        // Execute task on main thread.
        task(0, std::span<DatasetItem>{ memory.begin(), items_first_thread });

        // Wait for threads to finish.
        for (uint64_t tid = 1; tid < thread_count; ++tid) {
            threads[tid].join();
        }

        return memory;
    }
    
    namespace {
        // Calculates single DatasetItem (64-bytes of data) according to https://github.com/tevador/RandomX/blob/master/doc/specs.md#73-dataset-block-generation.
        DatasetItem generateItem(const argon2d::Memory& cache, const_span<Program, Rx_Cache_Accesses> programs, const uint64_t item_number) noexcept {
            // It can be assumed that cache size is static and equal to Rx_Argon2d_Memory_Blocks.
            ASSERTUME(cache.size() == Rx_Argon2d_Memory_Blocks);

            // 1. Initialize DatasetItem.
            DatasetItem dt_item{
                (item_number + 1) * 6364136223846793005ULL,
                ((item_number + 1) * 6364136223846793005ULL) ^ 9298411001130361340ULL,
                ((item_number + 1) * 6364136223846793005ULL) ^ 12065312585734608966ULL,
                ((item_number + 1) * 6364136223846793005ULL) ^ 9306329213124626780ULL,
                ((item_number + 1) * 6364136223846793005ULL) ^ 5281919268842080866ULL,
                ((item_number + 1) * 6364136223846793005ULL) ^ 10536153434571861004ULL,
                ((item_number + 1) * 6364136223846793005ULL) ^ 3398623926847679864ULL,
                ((item_number + 1) * 6364136223846793005ULL) ^ 9549104520008361294ULL,
            };

            // 2. Initialize cache index.
            constexpr uint32_t cache_item_count{ argon2d::Memory_Size / sizeof(DatasetItem) };
            auto cache_index{ item_number };
            DatasetItem cache_item{};

            // 3. For all programs...
            for (const auto& prog : programs) {
                // 4. Load cache item.
                const auto cache_offset{ (cache_index % cache_item_count) * sizeof(DatasetItem) };
                const auto block_index{ cache_offset / argon2d::Block_Size };
                const auto block_offset{ cache_offset % argon2d::Block_Size };
                std::memcpy(cache_item.data(), cache[block_index].data() + block_offset, sizeof(DatasetItem));

                // 5. Execute program with dt_item as registers.
                executeSuperscalar(dt_item, prog);

                // 6. XOR data and cache items.
                for (uint32_t j = 0; j < dt_item.size(); ++j) {
                    dt_item[j] ^= cache_item[j];
                }

                // 7. Set cache index
                cache_index = dt_item[prog.address_register];
            }

            return dt_item;
        }
    }
}
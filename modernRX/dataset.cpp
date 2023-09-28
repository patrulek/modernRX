#include <thread>

#include "argon2d.hpp"
#include "assertume.hpp"
#include "avx2.hpp"
#include "dataset.hpp"
#include "intrinsics.hpp"
#include "logger.hpp"
#include "superscalar.hpp"

namespace modernRX {
    using namespace intrinsics;

    namespace {
        [[nodiscard]] std::array<DatasetItem, 4> generate4Items(const argon2d::Memory& cache, const_span<SuperscalarProgram, Rx_Cache_Accesses> programs, const uint64_t item_number) noexcept;
    }

    std::vector<DatasetItem> generateDataset(const argon2d::Memory& cache, const_span<SuperscalarProgram, Rx_Cache_Accesses> programs) {
        // It can be assumed that cache size is static and equal to Rx_Argon2d_Memory_Blocks.
        ASSERTUME(cache.size() == Rx_Argon2d_Memory_Blocks);

        // Allocate memory for dataset.
        const uint32_t thread_count{ std::thread::hardware_concurrency() };

        // Dataset padding size adds additional memory to dataset to make it divisible by thread count * batch_size(4) without remainder.
        // This is needed to make sure that each thread will have the same amount of work and no additional function for handling remainders is needed.
        // Additional data will be ignored during hash calculation, its purpose is to simplify dataset generation.
        const uint32_t dataset_alignment{ thread_count * 4 * sizeof(DatasetItem) };
        const uint32_t dataset_padding_size{ dataset_alignment - ((Rx_Dataset_Base_Size + Rx_Dataset_Extra_Size) % dataset_alignment) };
        const uint32_t Dataset_Items_Count{ (Rx_Dataset_Base_Size + Rx_Dataset_Extra_Size + dataset_padding_size) / sizeof(DatasetItem) };
        std::vector<DatasetItem> memory(Dataset_Items_Count);

        const uint32_t items_per_thread{ Dataset_Items_Count / thread_count }; // Number of items per additional thread.

        // Assume that number of items per thread is always greater than 0. If not, invalid or exotic configuration is used.
        ASSERTUME(items_per_thread > 0);
        
        // Task that will be executed by each thread.
        const auto task = [&cache, &programs](const uint32_t tid, std::span<DatasetItem> submemory) {
            const uint32_t size{ static_cast<uint32_t>(submemory.size()) };
            uint32_t start_item{ tid * size };
            const uint32_t end_item{ start_item + size };

            // Generate DatasetItem's in batches of 4 items.
            for (size_t i = 0; i < submemory.size() / 4; ++i) {
                const auto items{ generate4Items(cache, programs, start_item) };
                std::memcpy(submemory.data() + i * 4, items.data(), sizeof(DatasetItem) * 4);
                start_item += 4;
            }
        };

        // Start threads.
        std::vector<std::thread> threads(thread_count);

        for (uint32_t tid = 1; tid < thread_count; ++tid) {
            threads[tid] = std::thread{ task, tid, std::span<DatasetItem>{ memory.begin() + tid * items_per_thread, items_per_thread } };
        }

        // Execute task on main thread.
        task(0, std::span<DatasetItem>{ memory.begin(), items_per_thread });

        // Wait for threads to finish.
        for (uint64_t tid = 1; tid < thread_count; ++tid) {
            threads[tid].join();
        }

        return memory;
    }
    
    namespace {
        // Calculates 4-batch DatasetItem (4x64-bytes of data) according to https://github.com/tevador/RandomX/blob/master/doc/specs.md#73-dataset-block-generation.
        // Enhanced by AVX2 intrinsics.
        std::array<DatasetItem, 4> generate4Items(const argon2d::Memory& cache, const_span<SuperscalarProgram, Rx_Cache_Accesses> programs, const uint64_t item_number) noexcept {
            // It can be assumed that cache size is fixed and equal to Rx_Argon2d_Memory_Blocks.
            ASSERTUME(cache.size() == Rx_Argon2d_Memory_Blocks);

            // 1. Initialize DatasetItem's with "register-wise" layout:
            // A0 B0 C0 D0
            // A1 B1 C1 D1
            // ...
            // A7 B7 C7 D7
            std::array<DatasetItem, 4> dt_items;

            // ymm_items is an avx2 view of dt_items.
            avx2::ymm<uint64_t>(&ymm_items)[8] { reinterpret_cast<avx2::ymm<uint64_t>(&)[8]>(dt_items) };

            const auto mul_consts{ avx2::vset<uint64_t>(6364136223846793005ULL) };
            const auto item_numbers{ avx2::vset<uint64_t>(item_number + 4, item_number + 3, item_number + 2, item_number + 1) };

            ymm_items[0] = avx2::vmul64<uint64_t>(item_numbers, mul_consts);
            ymm_items[1] = avx2::vxor<uint64_t>(ymm_items[0], avx2::vset<uint64_t>(9298411001130361340ULL));
            ymm_items[2] = avx2::vxor<uint64_t>(ymm_items[0], avx2::vset<uint64_t>(12065312585734608966ULL));
            ymm_items[3] = avx2::vxor<uint64_t>(ymm_items[0], avx2::vset<uint64_t>(9306329213124626780ULL));
            ymm_items[4] = avx2::vxor<uint64_t>(ymm_items[0], avx2::vset<uint64_t>(5281919268842080866ULL));
            ymm_items[5] = avx2::vxor<uint64_t>(ymm_items[0], avx2::vset<uint64_t>(10536153434571861004ULL));
            ymm_items[6] = avx2::vxor<uint64_t>(ymm_items[0], avx2::vset<uint64_t>(3398623926847679864ULL));
            ymm_items[7] = avx2::vxor<uint64_t>(ymm_items[0], avx2::vset<uint64_t>(9549104520008361294ULL));

            // 2. Initialize cache indexes.
            constexpr uint32_t cache_item_count{ argon2d::Memory_Size / sizeof(DatasetItem) };
            std::array<uint64_t, 4> cache_indexes{ item_number, item_number + 1, item_number + 2, item_number + 3 };
            std::array<const DatasetItem*, 4> cache_items_ptrs;

            // 3. For all programs...
            for (const auto& prog : programs) {
                for (uint32_t i = 0; i < 4; ++i) {
                    // 4. Prefetch cache items.
                    const auto cache_offset{ (cache_indexes[i] % cache_item_count) * sizeof(DatasetItem) };
                    const auto block_index{ cache_offset / argon2d::Block_Size };
                    const auto block_offset{ cache_offset % argon2d::Block_Size };
                    cache_items_ptrs[i] = intrinsics::prefetch<PrefetchMode::NTA, const DatasetItem*>(cache[block_index].data() + block_offset);
                }

                // 5. Execute program for 4-batch of DatasetItem's viewed as registers.
                executeSuperscalar(ymm_items, prog);

                // 6. XOR dataset and cache items.
                for (uint32_t i = 0; i < 4; ++i) {
                    const DatasetItem& cache_item{ *cache_items_ptrs[i] };

                    // Fallback to scalar code for simplicity.
                    dt_items[0][i] ^= cache_item[0];
                    dt_items[0][4 + i] ^= cache_item[1];

                    dt_items[1][i] ^= cache_item[2];
                    dt_items[1][4 + i] ^= cache_item[3];

                    dt_items[2][i] ^= cache_item[4];
                    dt_items[2][4 + i] ^= cache_item[5];

                    dt_items[3][i] ^= cache_item[6];
                    dt_items[3][4 + i] ^= cache_item[7];
                    
                    // 7. Set cache indexes. Prog.address_register is an index of given item's register so it needs to be mapped properly as at this point it's still "register-wise" layout.
                    cache_indexes[i] = dt_items[prog.address_register / 2][(prog.address_register % 2) * 4 + i];
                }
            }

            // Transpose DatasetItem's to "DatasetItem-wise" layout:
            // A0 A1 A2 A3 A4 A5 A6 A7
            // B0 B1 ...
            // C0 C1 ...
            // D0 D1 D2 D3 D4 D5 D6 D7
            avx2::vtranspose8x4pi64(ymm_items);
            return dt_items;
        }
    }
}
#include <thread>

#include "argon2d.hpp"
#include "assertume.hpp"
#include "avx2.hpp"
#include "dataset.hpp"
#include "logger.hpp"
#include "superscalar.hpp"


namespace modernRX {
    using namespace intrinsics;

    namespace {
        [[nodiscard]] DatasetItem generateItem(const argon2d::Memory& cache, const_span<Program, Rx_Cache_Accesses> programs, const uint32_t item_number) noexcept;
        [[nodiscard]] std::array<DatasetItem, 4> generate4Items(const argon2d::Memory& cache, const_span<Program, Rx_Cache_Accesses> programs, const uint64_t item_number) noexcept;
        void initializeItem(avx2::ymm<uint64_t>(&ymm_item)[2], const uint64_t item_number) noexcept;
    }

    std::vector<DatasetItem> generateDataset(const argon2d::Memory& cache, const_span<Program, Rx_Cache_Accesses> programs) {
        // It can be assumed that cache size is static and equal to Rx_Argon2d_Memory_Blocks.
        ASSERTUME(cache.size() == Rx_Argon2d_Memory_Blocks);

        // Allocate memory for dataset.
        constexpr uint32_t Dataset_Items_Count{ (Rx_Dataset_Base_Size + Rx_Dataset_Extra_Size) / sizeof(DatasetItem) };
        std::vector<DatasetItem> memory(Dataset_Items_Count);

        const uint32_t thread_count{ std::thread::hardware_concurrency() };
        const uint32_t items_per_thread{ Dataset_Items_Count / thread_count }; // Number of items per additional thread.
        const uint32_t items_first_thread{ items_per_thread + Dataset_Items_Count % thread_count }; // First thread will possibly have more items to process to make sure no item is left behind.
        
        // Task that will be executed by each thread.
        const auto task = [&cache, &programs, items_first_thread](const uint32_t tid, std::span<DatasetItem> submemory) {
            const uint32_t size{ static_cast<uint32_t>(submemory.size()) };
            uint32_t start_item{ items_first_thread + (tid - 1) * size };
            const uint32_t end_item{ start_item + size };

            // Do as much 4-batch calculations as possible.
            for (size_t i = 0; i < submemory.size() / 4; ++i) {
                const auto items{ generate4Items(cache, programs, start_item) };
                std::memcpy(submemory.data() + i * 4, items.data(), sizeof(DatasetItem) * 4);
                start_item += 4;
            }

            // Calculate remaining items.
            const uint64_t remainder{ submemory.size() % 4 };
            const uint64_t last_idx{ submemory.size() - remainder };

            switch (remainder) {
            case 3:
                submemory[last_idx + 2] = generateItem(cache, programs, start_item + 2);
                [[fallthrough]];
            case 2:
                submemory[last_idx + 1] = generateItem(cache, programs, start_item + 1);
                [[fallthrough]];
            case 1:
                submemory[last_idx] = generateItem(cache, programs, start_item);
                [[fallthrough]];
            case 0:
                break;
            default:
                std::unreachable();
            }
        };

        // Start threads.
        std::vector<std::thread> threads(thread_count);

        for (uint32_t tid = 1; tid < thread_count; ++tid) {
            threads[tid] = std::thread{ task, tid, std::span<DatasetItem>{ memory.begin() + items_first_thread + (tid - 1) * items_per_thread, items_per_thread } };
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
        // Enhanced by AVX2 intrinsics.
        DatasetItem generateItem(const argon2d::Memory& cache, const_span<Program, Rx_Cache_Accesses> programs, const uint32_t item_number) noexcept {
            // It can be assumed that cache size is fixed and equal to Rx_Argon2d_Memory_Blocks.
            ASSERTUME(cache.size() == Rx_Argon2d_Memory_Blocks);

            // 1. Initialize DatasetItem.
            DatasetItem dt_item;
            avx2::ymm<uint64_t>(&ymm_item)[2] { reinterpret_cast<avx2::ymm<uint64_t>(&)[2]>(dt_item) };
            initializeItem(ymm_item, item_number);

            // 2. Initialize cache index.
            constexpr uint32_t cache_item_count{ argon2d::Memory_Size / sizeof(DatasetItem) };
            uint64_t cache_index{ item_number };
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
                ymm_item[0] = avx2::vxor<uint64_t>(ymm_item[0], (avx2::ymm<uint64_t>&)cache_item[0]);
                ymm_item[1] = avx2::vxor<uint64_t>(ymm_item[1], (avx2::ymm<uint64_t>&)cache_item[4]);

                // 7. Set cache index
                cache_index = dt_item[prog.address_register];
            }

            return dt_item;
        }

        // Calculates 4-batch DatasetItem (4x64-bytes of data) according to https://github.com/tevador/RandomX/blob/master/doc/specs.md#73-dataset-block-generation.
        // Enhanced by AVX2 intrinsics.
        std::array<DatasetItem, 4> generate4Items(const argon2d::Memory& cache, const_span<Program, Rx_Cache_Accesses> programs, const uint64_t item_number) noexcept {
            // It can be assumed that cache size is fixed and equal to Rx_Argon2d_Memory_Blocks.
            ASSERTUME(cache.size() == Rx_Argon2d_Memory_Blocks);

            // 1. Initialize DatasetItem.
            std::array<DatasetItem, 4> dt_items;
            avx2::ymm<uint64_t>(&ymm_items)[4][2] { reinterpret_cast<avx2::ymm<uint64_t>(&)[4][2]>(dt_items) };
            initializeItem(ymm_items[0], item_number);
            initializeItem(ymm_items[1], item_number + 1);
            initializeItem(ymm_items[2], item_number + 2);
            initializeItem(ymm_items[3], item_number + 3);

            // 2. Initialize cache index.
            constexpr uint32_t cache_item_count{ argon2d::Memory_Size / sizeof(DatasetItem) };
            std::array<uint64_t, 4> cache_indexes{ item_number, item_number + 1, item_number + 2, item_number + 3 };
            std::array<DatasetItem, 4> cache_items{};

            // 3. For all programs...
            for (const auto& prog : programs) {
                for (uint32_t i = 0; i < 4; ++i) {
                    // 4. Load cache item.
                    const auto cache_offset{ (cache_indexes[i] % cache_item_count) * sizeof(DatasetItem)};
                    const auto block_index{ cache_offset / argon2d::Block_Size };
                    const auto block_offset{ cache_offset % argon2d::Block_Size };
                    std::memcpy(cache_items[i].data(), cache[block_index].data() + block_offset, sizeof(DatasetItem));
                }

                // 5. Execute program with dt_item as registers.
                for (uint32_t i = 0; i < 4; ++i) {
                    executeSuperscalar(dt_items[i], prog);
                }

                // 6. XOR data and cache items.
                for (uint32_t i = 0; i < 4; ++i) {
                    ymm_items[i][0] = avx2::vxor<uint64_t>(ymm_items[i][0], (avx2::ymm<uint64_t>&)cache_items[i][0]);
                    ymm_items[i][1] = avx2::vxor<uint64_t>(ymm_items[i][1], (avx2::ymm<uint64_t>&)cache_items[i][4]);
                    
                    // 7. Set cache index
                    cache_indexes[i] = dt_items[i][prog.address_register];
                }
            }

            return dt_items;
        }

        // Performs first step of DatasetItem initialization according to: https://github.com/tevador/RandomX/blob/master/doc/specs.md#73-dataset-block-generation
        // Uses AVX2 intrinsics.
        void initializeItem(avx2::ymm<uint64_t>(&ymm_item)[2], const uint64_t item_number) noexcept {
            const auto item_numbers{ avx2::vset<uint64_t>(item_number + 1, item_number + 1, item_number + 1, item_number + 1) };
            const auto mul_consts{ avx2::vset<uint64_t>(0x5851F42D4C957F2D, 0x5851F42D4C957F2D, 0x5851F42D4C957F2D, 0x5851F42D4C957F2D) };
            const avx2::ymm<uint64_t> xor_consts[2]{
                avx2::vset<uint64_t>(9306329213124626780ULL, 12065312585734608966ULL, 9298411001130361340ULL, 0ULL),
                avx2::vset<uint64_t>(9549104520008361294ULL, 3398623926847679864ULL, 10536153434571861004ULL, 5281919268842080866ULL)
            };

            ymm_item[0] = avx2::vmul64<uint64_t>(item_numbers, mul_consts);
            ymm_item[0] = avx2::vxor<uint64_t>(ymm_item[0], xor_consts[0]);

            ymm_item[1] = avx2::vmul64<uint64_t>(item_numbers, mul_consts);
            ymm_item[1] = avx2::vxor<uint64_t>(ymm_item[1], xor_consts[1]);
        }
    }
}
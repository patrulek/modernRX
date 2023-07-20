#include "dataset.hpp"

#include "utils.hpp"
#include "reciprocal.hpp"
#include "logger.hpp"
#include <chrono>

namespace modernRX {
    namespace {
        DatasetItem generateItem(const argon2d::Memory& cache_memory, const std::array<Program, Program_Count>& programs, const uint64_t item_number);
    }

    DatasetMemory generateDataset(const argon2d::Memory& cache_memory, const std::array<Program, Program_Count>& programs) {
        static constexpr uint32_t Dataset_Base_Size{ 2'147'483'648 };
        static constexpr uint32_t Dataset_Extra_Size{ 33'554'432 };
        static constexpr uint32_t Dataset_Items_Count{ (Dataset_Base_Size + Dataset_Extra_Size) / sizeof(DatasetItem) };

        DatasetMemory memory;
        memory.reserve(Dataset_Items_Count);

        for (uint32_t i = 0; i < Dataset_Items_Count; i++) {
            const auto item = generateItem(cache_memory, programs, i);
            memory.push_back(item);

            if (i % 1'000'000 == 0) {
                std::chrono::time_point<std::chrono::utc_clock> time{ std::chrono::utc_clock::now() };
                slog("time", std::format("{:%T}", time), "dataset index", i, "value", std::format("0x{:x}", memory.back()[0]));
            }
        }

        return memory;
    }
    
    namespace {
        // Calculates single DatasetItem (64-bytes of data) according to https://github.com/tevador/RandomX/blob/master/doc/specs.md#73-dataset-block-generation.
        DatasetItem generateItem(const argon2d::Memory& cache_memory, const std::array<Program, Program_Count>& programs, const uint64_t item_number) {
            // 1. Initialize DatasetItem.
            DatasetItem dt_item = DatasetItem{
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
            auto cache_index{ item_number };
            const auto cache_item_count{ argon2d::Block_Size * cache_memory.size() / sizeof(DatasetItem) };
            DatasetItem cache_item{};

            // 3. For all programs...
            for (const auto& prog : programs) {
                // 4. Load cache item.
                const auto cache_offset{ (cache_index % cache_item_count) * sizeof(DatasetItem) };
                const auto block_index{ cache_offset / argon2d::Block_Size };
                const auto block_offset{ cache_offset % argon2d::Block_Size };
                std::memcpy(cache_item.data(), cache_memory[block_index].data() + block_offset, sizeof(DatasetItem));

                // 5. Execute program with dt_item as registers.
                executeSuperscalar(dt_item, prog);

                // 6. XOR data and cache items.
                for (uint32_t j = 0; j < dt_item.size(); j++) {
                    dt_item[j] ^= cache_item[j];
                }

                // 7. Set cache index
                cache_index = dt_item[prog.address_register];
            }

            return dt_item;
        }
    }
}
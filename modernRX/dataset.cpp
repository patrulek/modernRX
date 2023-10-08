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
        constexpr uint32_t cache_item_count{ argon2d::Memory_Size / sizeof(DatasetItem) };
        static_assert(std::has_single_bit(cache_item_count)); // This is guaranteed by algorithm specification, but let's keep it here just in case.

        [[nodiscard]] std::array<DatasetItem, 4> generate4Items(const_span<argon2d::Block> cache, const_span<SuperscalarProgram, Rx_Cache_Accesses> programs, const avx2::ymm<uint64_t> ymmitem0, avx2::ymm<uint64_t> cache_indexes) noexcept;
    }

    HeapArray<DatasetItem, 4096> generateDataset(const_span<argon2d::Block> cache, const_span<SuperscalarProgram, Rx_Cache_Accesses> programs) {
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
        HeapArray<DatasetItem, 4096> memory(Dataset_Items_Count);

        const uint32_t items_per_thread{ Dataset_Items_Count / thread_count }; // Number of items per additional thread.

        // Assume that number of items per thread is always greater than 0. If not, invalid or exotic configuration is used.
        ASSERTUME(items_per_thread > 0);
        
        // Task that will be executed by each thread.
        const auto task = [&cache, &programs](const uint32_t tid, std::span<DatasetItem> submemory) {
            const uint32_t size{ static_cast<uint32_t>(submemory.size()) };
            uint32_t start_item{ tid * size };
            const uint32_t end_item{ start_item + size };

            const auto mul_consts{ avx2::vset<uint64_t>(6364136223846793005ULL) };
            const auto add_consts{ avx2::vset<uint64_t>(7009800821677620404ULL) }; // 4 * mul_consts
            const auto item_numbers{ avx2::vset<uint64_t>(start_item + 4, start_item + 3, start_item + 2, start_item + 1) };

            // 1. Initialize first register of DatasetItem's.
            auto ymmitem0{ avx2::vmul64<uint64_t>(item_numbers, mul_consts) };

            // 2. Initialize cache indexes.
            auto cache_indexes{ avx2::vset<uint64_t>(start_item + 3, start_item + 2, start_item + 1, start_item) };
             
            // Generate DatasetItem's in batches of 4 items.
            for (size_t i = 0; i < submemory.size() / 4; ++i) {
                const auto items{ generate4Items(cache, programs, ymmitem0, cache_indexes) };
                std::memcpy(submemory.data() + i * 4, items.data(), sizeof(DatasetItem) * 4);
                ymmitem0 = avx2::vadd<uint64_t>(ymmitem0, add_consts);
                cache_indexes = avx2::vadd<uint64_t>(cache_indexes, avx2::vset<uint64_t>(4));
                start_item += 4;
            }
        };

        // Start threads.
        std::vector<std::thread> threads(thread_count);

        for (uint32_t tid = 1; tid < thread_count; ++tid) {
            threads[tid] = std::thread{ task, tid, memory.buffer(tid * items_per_thread, items_per_thread) };
        }

        // Execute task on main thread.
        task(0, memory.buffer(0, items_per_thread));

        // Wait for threads to finish.
        for (uint64_t tid = 1; tid < thread_count; ++tid) {
            threads[tid].join();
        }

        return memory;
    }
    
    namespace {
        // Calculates 4-batch DatasetItem (4x64-bytes of data) according to https://github.com/tevador/RandomX/blob/master/doc/specs.md#73-dataset-block-generation.
        // Enhanced by AVX2 intrinsics.
        std::array<DatasetItem, 4> generate4Items(const_span<argon2d::Block> cache, const_span<SuperscalarProgram, Rx_Cache_Accesses> programs, const avx2::ymm<uint64_t> ymmitem0, avx2::ymm<uint64_t> cache_indexes) noexcept {
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

            ymm_items[0] = ymmitem0;
            ymm_items[1] = avx2::vxor<uint64_t>(ymmitem0, avx2::vset<uint64_t>(9298411001130361340ULL));
            ymm_items[2] = avx2::vxor<uint64_t>(ymmitem0, avx2::vset<uint64_t>(12065312585734608966ULL));
            ymm_items[3] = avx2::vxor<uint64_t>(ymmitem0, avx2::vset<uint64_t>(9306329213124626780ULL));
            ymm_items[4] = avx2::vxor<uint64_t>(ymmitem0, avx2::vset<uint64_t>(5281919268842080866ULL));
            ymm_items[5] = avx2::vxor<uint64_t>(ymmitem0, avx2::vset<uint64_t>(10536153434571861004ULL));
            ymm_items[6] = avx2::vxor<uint64_t>(ymmitem0, avx2::vset<uint64_t>(3398623926847679864ULL));
            ymm_items[7] = avx2::vxor<uint64_t>(ymmitem0, avx2::vset<uint64_t>(9549104520008361294ULL));

            std::array<const DatasetItem*, 4> cache_items_ptrs;
            const auto cache_ptr{ avx2::vset<uint64_t>(reinterpret_cast<uint64_t>(cache.data())) };

            // 3. For all programs...
            for (const auto& prog : programs) {
                // 4. Prefetch cache items.
                constexpr uint32_t dataset_item_size_shift{ 6 }; // 2^6 = 64 -> sizeof(DatasetItem)
                // Cache offsets are calculated as (cache_indexes % cache_item_count) * sizeof(DatasetItem).
                auto cache_offsets{ avx2::vand<uint64_t>(cache_indexes, avx2::vset<uint64_t>(cache_item_count - 1)) };
                cache_offsets = avx2::vlshift<uint64_t>(cache_offsets, dataset_item_size_shift);
                cache_offsets = avx2::vadd<uint64_t>(cache_offsets, cache_ptr);
                cache_items_ptrs[0] = intrinsics::prefetch<PrefetchMode::NTA, const DatasetItem*>(reinterpret_cast<const std::byte*>(cache_offsets.m256i_u64[0]));
                cache_items_ptrs[1] = intrinsics::prefetch<PrefetchMode::NTA, const DatasetItem*>(reinterpret_cast<const std::byte*>(cache_offsets.m256i_u64[1]));
                cache_items_ptrs[2] = intrinsics::prefetch<PrefetchMode::NTA, const DatasetItem*>(reinterpret_cast<const std::byte*>(cache_offsets.m256i_u64[2]));
                cache_items_ptrs[3] = intrinsics::prefetch<PrefetchMode::NTA, const DatasetItem*>(reinterpret_cast<const std::byte*>(cache_offsets.m256i_u64[3]));

                // 5. Execute program for 4-batch of DatasetItem's viewed as registers.
                executeSuperscalar(ymm_items, prog);

                // 6. XOR dataset and cache items.
                const DatasetItem& cache_item{ *cache_items_ptrs[0] };
                const DatasetItem& cache_item1{ *cache_items_ptrs[1] };
                const DatasetItem& cache_item2{ *cache_items_ptrs[2] };
                const DatasetItem& cache_item3{ *cache_items_ptrs[3] };

                // Transpose forth and back to make XOR for registers 0-3.
                auto tmp = avx2::vunpackloepi64<uint64_t>(ymm_items[0], ymm_items[1]); // tmp = A0 A1 C0 C1
                auto tmp2 = avx2::vunpackloepi64<uint64_t>(ymm_items[2], ymm_items[3]); // tmp2 = A2 A3 C2 C3
                auto tmp3 = avx2::vpermute2x128<uint64_t, 0x20>(tmp, tmp2); // tmp3 = A0 A1 A2 A3
                tmp3 = avx2::vxor<uint64_t>(tmp3, avx2::vload256<uint64_t>(cache_item.data()));
                auto tmp4 = avx2::vpermute2x128<uint64_t, 0x31>(tmp, tmp2); // tmp4 = C0 C1 C2 C3
                tmp4 = avx2::vxor<uint64_t>(tmp4, avx2::vload256<uint64_t>(cache_item2.data()));

                auto tmp5 = avx2::vunpackhiepi64<uint64_t>(ymm_items[0], ymm_items[1]); // tmp5 = B0 B1 D0 D1
                auto tmp6 = avx2::vunpackhiepi64<uint64_t>(ymm_items[2], ymm_items[3]); // tmp6 = B2 B3 D2 D3
                auto tmp7 = avx2::vpermute2x128<uint64_t, 0x20>(tmp5, tmp6); // tmp7 = B0 B1 B2 B3
                tmp7 = avx2::vxor<uint64_t>(tmp7, avx2::vload256<uint64_t>(cache_item1.data()));
                auto tmp8 = avx2::vpermute2x128<uint64_t, 0x31>(tmp5, tmp6); // tmp8 = D0 D1 D2 D3
                tmp8 = avx2::vxor<uint64_t>(tmp8, avx2::vload256<uint64_t>(cache_item3.data()));

                tmp = avx2::vunpackloepi64<uint64_t>(tmp3, tmp7); // tmp = A0 B0 A2 B2
                tmp2 = avx2::vunpackloepi64<uint64_t>(tmp4, tmp8); // tmp2 = C0 D0 C2 D2
                ymm_items[0] = avx2::vpermute2x128<uint64_t, 0x20>(tmp, tmp2); // ymm_items[0] = A0 B0 C0 D0
                ymm_items[2] = avx2::vpermute2x128<uint64_t, 0x31>(tmp, tmp2); // ymm_items[2] = A2 B2 C2 D2

                tmp = avx2::vunpackhiepi64<uint64_t>(tmp3, tmp7); // tmp = A1 B1 A3 B3
                tmp2 = avx2::vunpackhiepi64<uint64_t>(tmp4, tmp8); // tmp2 = C1 D1 C3 D3
                ymm_items[1] = avx2::vpermute2x128<uint64_t, 0x20>(tmp, tmp2); // ymm_items[1] = A1 B1 C1 D1
                ymm_items[3] = avx2::vpermute2x128<uint64_t, 0x31>(tmp, tmp2); // ymm_items[3] = A3 B3 C3 D3


                // Transpose forth and back to make XOR for registers 4-7.
                tmp = avx2::vunpackloepi64<uint64_t>(ymm_items[4], ymm_items[5]); // tmp = A4 A5 C4 C5
                tmp2 = avx2::vunpackloepi64<uint64_t>(ymm_items[6], ymm_items[7]); // tmp2 = A6 A7 C6 C7
                tmp3 = avx2::vpermute2x128<uint64_t, 0x20>(tmp, tmp2); // tmp3 = A4 A5 A6 A7
                tmp3 = avx2::vxor<uint64_t>(tmp3, avx2::vload256<uint64_t>(cache_item.data() + 4));
                tmp4 = avx2::vpermute2x128<uint64_t, 0x31>(tmp, tmp2); // tmp4 = C4 C5 C6 C7
                tmp4 = avx2::vxor<uint64_t>(tmp4, avx2::vload256<uint64_t>(cache_item2.data() + 4));

                tmp5 = avx2::vunpackhiepi64<uint64_t>(ymm_items[4], ymm_items[5]); // tmp = B4 B5 D4 D5
                tmp6 = avx2::vunpackhiepi64<uint64_t>(ymm_items[6], ymm_items[7]); // tmp2 = B6 B7 D6 D7
                tmp7 = avx2::vpermute2x128<uint64_t, 0x20>(tmp5, tmp6); // tmp3 = B4 B5 B6 B7
                tmp7 = avx2::vxor<uint64_t>(tmp7, avx2::vload256<uint64_t>(cache_item1.data() + 4));
                tmp8 = avx2::vpermute2x128<uint64_t, 0x31>(tmp5, tmp6); // tmp4 = D4 D5 D6 D7
                tmp8 = avx2::vxor<uint64_t>(tmp8, avx2::vload256<uint64_t>(cache_item3.data() + 4));

                tmp = avx2::vunpackloepi64<uint64_t>(tmp3, tmp7); // tmp = A4 B4 A6 B6
                tmp2 = avx2::vunpackloepi64<uint64_t>(tmp4, tmp8); // tmp2 = C4 D4 C6 D6
                ymm_items[4] = avx2::vpermute2x128<uint64_t, 0x20>(tmp, tmp2); // ymm_items[0] = A4 B4 C4 D4
                ymm_items[6] = avx2::vpermute2x128<uint64_t, 0x31>(tmp, tmp2); // ymm_items[2] = A6 B6 C6 D6

                tmp = avx2::vunpackhiepi64<uint64_t>(tmp3, tmp7); // tmp = A5 B5 A7 B7
                tmp2 = avx2::vunpackhiepi64<uint64_t>(tmp4, tmp8); // tmp2 = C5 D5 C7 D7
                ymm_items[5] = avx2::vpermute2x128<uint64_t, 0x20>(tmp, tmp2); // ymm_items[1] = A5 B5 C5 D5
                ymm_items[7] = avx2::vpermute2x128<uint64_t, 0x31>(tmp, tmp2); // ymm_items[3] = A7 B7 C7 D7

                // 7. Set cache indexes.
                cache_indexes = ymm_items[prog.address_register];
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
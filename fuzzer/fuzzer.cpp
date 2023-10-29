#include "hasher.hpp"
#include "3rdparty/RandomX/src/dataset.hpp"
#include "3rdparty/RandomX/src/jit_compiler.hpp"

using namespace modernRX;


extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    constexpr size_t Max_Data_Size{ Rx_Block_Template_Size };
    std::array<std::byte, Max_Data_Size> key_or_data;

    for (size_t i = 0; i + size <= Max_Data_Size && size != 0; i += size) {
        std::memcpy(key_or_data.data() + i, data, size);
    }

    // Initialize modernRX hasher
    Hasher hasher{ key_or_data };

    // Initialize RandomX dataset and VM
    auto initThreadCount{ std::thread::hardware_concurrency() };
    auto flags{ randomx_get_flags() | RANDOMX_FLAG_FULL_MEM };
    auto cache{ randomx_alloc_cache(flags) };
    randomx_init_cache(cache, key_or_data.data(), key_or_data.size());

    auto dataset{ randomx_alloc_dataset(flags) };
    auto datasetItemCount{ randomx_dataset_item_count() };
    auto perThread{ datasetItemCount / initThreadCount };
    auto remainder{ datasetItemCount % initThreadCount };
    auto startItem{ 0 };
    std::vector<std::thread> threads;

    for (int i = 0; i < initThreadCount; ++i) {
        auto count{ perThread + (i == initThreadCount - 1 ? remainder : 0) };
        threads.push_back(std::thread(&randomx_init_dataset, dataset, cache, startItem, count));
        startItem += count;
    }

    for (unsigned i = 0; i < threads.size(); ++i) {
        threads[i].join();
    }

    randomx_vm* vm = randomx_create_vm(flags, cache, dataset);
    std::array<std::byte, RANDOMX_HASH_SIZE> randomx_hash{};

    // Calculate hashes for modernRX and RandomX to compare.
    for (int i = 0; i < size; ++i) {
        auto modernRX_hash{ hasher.run(key_or_data) };
        randomx_calculate_hash(vm, key_or_data.data(), key_or_data.size(), randomx_hash.data());

        if (!std::equal(modernRX_hash.begin(), modernRX_hash.end(), randomx_hash.begin())) {
            std::abort();
        }

        key_or_data[(i + size) % Max_Data_Size] = static_cast<std::byte>(static_cast<uint8_t>(key_or_data[(i + size) % Max_Data_Size]) + 1);
    }

    randomx_release_cache(cache);
    randomx_release_dataset(dataset);
    randomx_destroy_vm(vm);

    return 0;
}

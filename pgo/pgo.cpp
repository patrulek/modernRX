#include <algorithm>
#include <print>
#include <random>

#include "hasher.hpp"

int main(int argc, char *argv[]) {
    std::random_device rd;
    std::mt19937 gen{ rd() };

    std::array<std::byte, modernRX::Rx_Block_Template_Size> block_template{};
    std::generate(block_template.begin(), block_template.end(), [&gen]() {
        return std::byte{ static_cast<uint8_t>(gen()) };
    });

    std::println("Profiling modernRX...");

    try {
        modernRX::Hasher hasher{ block_template };

        for (size_t i = 0; i < 150; ++i) {
            [[maybe_unused]] auto _ { hasher.run(block_template) };
            block_template[i % block_template.size()] = static_cast<std::byte>(static_cast<uint8_t>(block_template[i]) + 1);
        }
    } catch (const modernRX::Exception &ex) {
        std::println("{}", ex.what());
        return -1;
    }

    return 0;
}

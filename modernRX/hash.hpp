#pragma once

#include <atomic>

#include "randomxparams.hpp"

namespace modernRX {
    struct RxHash {
        std::array<uint8_t, Rx_Hash_Size> data;

        std::span<std::byte> buffer() {
            return { reinterpret_cast<std::byte*>(data.data()), Rx_Hash_Size };
        }

        const bool operator==(const RxHash& other) const {
            return data == other.data;
        }
    };
}

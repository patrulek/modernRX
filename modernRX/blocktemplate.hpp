#pragma once

#include "randomxparams.hpp"

namespace modernRX {
    struct BlockTemplate {
        static constexpr uint32_t Rx_Block_Template_Nonce_Offset{ 39 };

        uint8_t data[Rx_Block_Template_Size];

        // Increases nonce by offset.
        void next(const uint32_t offset = 1) {
            reinterpret_cast<uint32_t&>(data[Rx_Block_Template_Nonce_Offset]) += offset;
        }

        const_span<std::byte> view() const {
            return { reinterpret_cast<const std::byte*>(data), Rx_Block_Template_Size };
        }
    };
}
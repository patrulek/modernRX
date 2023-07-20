#pragma once

#include <string_view>
#include <format>
#include <array>
#include <print>

#ifdef _DEBUG
static constexpr bool enabled{ true };
#else
static constexpr bool enabled{ false };
#endif

template<bool Enabled = enabled, typename ...Args>
constexpr void slog(Args&&... args) {
    static_assert(sizeof...(Args) % 2 == 0 && sizeof...(Args) >= 2, "logger requires whole key-value pairs");
    if constexpr (!Enabled) {
        return;
    }

    constexpr int n{ sizeof...(Args) / 2 };
    constexpr std::string_view source{ "\"{:s}\"={}, " };

    static constexpr std::array<char, 2 + source.size() * n> fmt = [&n]() consteval {
        std::array<char, 2 + source.size() * n> buf{};
        buf[0] = buf[1] = '{';

        for  (size_t i = 2; i < 2 + source.size() * n; i+=source.size()) {
            for (size_t j = 0; j < source.size(); ++j) {
                buf[i + j] = source[j];
            }
        }

        buf[source.size() * n] = buf[source.size() * n + 1] = '}';
        return buf;
    }();

    std::println(std::string_view{ fmt }, args...);
}
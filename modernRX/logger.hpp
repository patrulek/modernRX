#pragma once

/*
* Simple, structured logger for debugging purposes only.
* Not a part of RandomX algorithm.
*/

#include <array>
#include <print>
#include <string_view>

// This is exception from a rule to avoid using preprocessor.
// It's for convenience only as it allows for easy enabling logging in debug builds.
#ifdef _DEBUG
static constexpr bool enabled{ true };
#else
static constexpr bool enabled{ false };
#endif

// Prints key-value pairs in a structured, JSON-like format.
// Requires even number of arguments, where every odd argument is a key and every even argument is a value.
//
// Example usage:
// slog("some_key", 123, "another_key", true, "ok_boomer", "i get it"); <- will print log depending on enabled flag
// slog<true>("some_key", 123, "another_key", true, "ok_boomer", "i get it"); <- will always print log regardless of enabled flag
template<bool Enabled = enabled, typename ...Args>
constexpr void slog(Args&&... args) {
    static_assert(sizeof...(Args) % 2 == 0 && sizeof...(Args) >= 2, "Logger requires whole key-value pairs.");
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
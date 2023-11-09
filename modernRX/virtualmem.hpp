#pragma once

/*
* Wrapper over Windows virtual memory API.
* Used to allocate executable memory for JIT-compiled programs.
*/

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdint>
#include <format>

#include "aliases.hpp"
#include "exception.hpp"

namespace modernRX {
    template<typename T>
    inline constexpr bool is_vector_v = std::same_as<T, std::vector<typename T::value_type, typename T::allocator_type> >;

    template<typename T>
    inline constexpr bool is_spanable_v = std::is_constructible_v<const_span<typename T::value_type>, T>;

    template<typename T>
    [[nodiscard]] constexpr const_span<typename T::value_type> as_span(T t) noexcept {
        return const_span<typename T::value_type>(t);
    }

    // Allocates executable memory and returns a function pointer to it.
    // Type of the function pointer is specified by the template parameter.
    // The function pointer is wrapped in a unique_ptr with a custom deleter that will free allocated memory and data associated with program at destruction.
    // May throw if memory allocation or protection fails.
    // 
    // Function takes data as a parameter and prolongs its lifetime until the JIT-compiled function is destroyed.
    // Somewhat hacky, would be nice to find a better solution.
    template<typename Fn, typename Code, typename Data>
    requires is_spanable_v<Code> && is_vector_v<Data>
    [[nodiscard]] constexpr jit_function_ptr<Fn> makeExecutable(const Code code, Data&& data) {
        const auto code_size{ as_span(code).size_bytes() };

        // Alloc buffer for writing code.
        auto buffer{ VirtualAlloc(nullptr, code_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) };
        if (buffer == nullptr) {
            throw Exception(std::format("Failed to allocate memory with error: {:d}", GetLastError()));
        }

        std::memcpy(buffer, code.data(), code_size);

        // Protect from writing, but make code executable.
        DWORD dummy{};
        if (!VirtualProtect(buffer, code_size, PAGE_EXECUTE_READ, &dummy)) {
            const auto err{ GetLastError() };
            VirtualFree(buffer, 0, MEM_RELEASE); // Ignore error.
            throw Exception(std::format("Failed to protect memory with error: {:d}", err));
        }

        return jit_function_ptr<Fn>(reinterpret_cast<Fn*>(buffer), [moved_data = std::move(data)](Fn* ptr) noexcept {
            VirtualFree(ptr, 0, MEM_RELEASE); // Ignore error.
            // moved_data will be destroyed and release memory here automatically.
        });
    }
}

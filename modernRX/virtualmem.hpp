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
#include <stdexcept>

#include "aliases.hpp"

namespace modernRX {
    // Allocates executable memory and returns a function pointer to it.
    // Type of the function pointer is specified by the template parameter.
    // The function pointer is wrapped in a unique_ptr with a custom deleter that will free allocated memory at destruction.
    // May throw if memory allocation or protection fails.
    // Deleter dont throw on failure.
    template<typename Fn, typename Byte = std::byte>
    requires (sizeof(Byte) == 1 && std::is_trivially_copyable_v<Byte>)
    jit_function_ptr<Fn> makeExecutable(const_span<Byte> code) {
        // Alloc buffer for writing code.
        auto buffer{ VirtualAlloc(nullptr, code.size_bytes(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) };
        if (buffer == nullptr) {
            throw std::runtime_error(std::format("Failed to allocate memory with error: {:d}", GetLastError()));
        }

        std::memcpy(buffer, code.data(), code.size_bytes());

        // Protect from writing, but make executable.
        DWORD dummy{};
        if (!VirtualProtect(buffer, code.size_bytes(), PAGE_EXECUTE_READ, &dummy)) {
            VirtualFree(buffer, 0, MEM_RELEASE);
            throw std::runtime_error(std::format("Failed to protect memory with error: {:d}", GetLastError()));
        }

        Fn* fn{ new Fn };
        *fn = reinterpret_cast<Fn>(buffer);
        return jit_function_ptr<Fn>(fn, [](Fn* ptr) noexcept {
            VirtualFree(*ptr, 0, MEM_RELEASE); // TODO: What to do with error?
            *ptr = nullptr;
            delete ptr;
            ptr = nullptr;
        });
    }
}
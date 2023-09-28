#pragma once

/*
* Convenient aliases for std types.
* Not a part of RandomX algorithm.
*/

#include <array>
#include <functional>
#include <memory>
#include <numeric>
#include <span>

// Convenient alias for `const std::span<const T>`.
template<typename T, size_t Size = std::numeric_limits<size_t>::max()>
using const_span = const std::span<const T, Size>;

// Convenient alias for `const std::array<const T, Size>`.
template<typename T, size_t Size>
using const_array = const std::array<const T, Size>;

// Convenient alias for storing dynamically (JIT) compiled program as function pointer in std::unique_ptr.
template<typename Fn>
requires std::is_pointer_v<Fn> && std::is_function_v<std::remove_pointer_t<Fn>>
using jit_function_ptr = std::unique_ptr<Fn, std::function<void(Fn*)>>;
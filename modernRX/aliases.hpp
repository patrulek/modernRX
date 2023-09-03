#pragma once

/*
* Convenient aliases for std types.
* Not a part of RandomX algorithm.
*/

#include <array>
#include <numeric>
#include <span>

// Convenient alias for `const std::span<const T>`.
template<typename T, size_t Size = std::numeric_limits<size_t>::max()>
using const_span = const std::span<const T>;

// Convenient alias for `const std::array<const T, Size>`.
template<typename T, size_t Size>
using const_array = const std::array<const T, Size>;
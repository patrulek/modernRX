#pragma once

/*
* Some utility functions used for casting between types.
* Not a part of RandomX algorithm.
*/

#include <bit>
#include <concepts>
#include <numeric>
#include <span>
#include <utility>
#include <vector>

#include "aliases.hpp"

// Casts one const_span to another with different type and proportionally different size.
template<typename T, size_t Size = std::numeric_limits<size_t>::max(), typename Val, size_t Input_Size = std::numeric_limits<size_t>::max()>
[[nodiscard]] constexpr const_span<T, Size> span_cast(const_span<Val, Input_Size> v) noexcept {
	static_assert(Size * sizeof(T) == Input_Size * sizeof(Val));
	return const_span<T, Size>(reinterpret_cast<const T*>(v.data()), Size);
}

// Casts a byte pointer to a span of given type and size.
template<typename T, size_t Size = std::numeric_limits<size_t>::max()>
[[nodiscard]] constexpr std::span<T, Size> span_cast(std::byte* ptr) noexcept {
	return std::span<T, Size>(reinterpret_cast<T*>(ptr), Size);
}

// Casts a const byte pointer to a const_span of given type and size.
template<typename T, size_t Size = std::numeric_limits<size_t>::max()>
[[nodiscard]] constexpr const_span<T, Size> span_cast(const std::byte* ptr) noexcept {
	return const_span<T, Size>(reinterpret_cast<const T*>(ptr), Size);
}

// Casts a const value of any type to a span of given type and size.
template<typename T, size_t Size = std::numeric_limits<size_t>::max(), typename Val>
[[nodiscard]] constexpr std::span<T, Size> span_cast(const Val& v) noexcept {
	static_assert(Size == std::numeric_limits<size_t>::max() || Size * sizeof(T) <= sizeof(Val));
	constexpr size_t n{ std::min(Size, sizeof(Val) / sizeof(T)) };

	return const_span<T, Size>(reinterpret_cast<const T*>(&v), n);
}

// Casts a const value of any type to a span of given type and size.
template<typename T, size_t Size = std::numeric_limits<size_t>::max(), typename Val>
[[nodiscard]] constexpr std::span<T, Size> span_cast(Val& v) noexcept {
	static_assert(Size == std::numeric_limits<size_t>::max() || Size * sizeof(T) <= sizeof(Val));
	constexpr size_t n{ std::min(Size, sizeof(Val) / sizeof(T)) };

	return std::span<T, Size>(reinterpret_cast<T*>(&v), n);
}

// Creates std::array of bytes from given arguments.
// Arguments values must be in range of a single byte.
template<typename ...Args>
[[nodiscard]] consteval std::array<std::byte, sizeof...(Args)> byte_array(const Args&&... args) noexcept {
	// Workaround over "fold expression did not evaluate to a constant" error
	if (((args > std::numeric_limits<uint8_t>::max()) || ... )) {
		throw "byte_array requires all arguments to be in single byte value range";
    }

	return std::array<std::byte, sizeof...(Args)>{ static_cast<const std::byte>(args)... };
}

// Creates std::vector of bytes from given arguments.
// Arguments values must be in range of a single byte.
// This will check all arguments at runtime, not compile-time.
template<typename ...Args>
[[nodiscard]] constexpr std::vector<std::byte> byte_vector(const Args&&... args) {
	// Workaround over "fold expression did not evaluate to a constant" error
	if (((args > std::numeric_limits<uint8_t>::max()) || ...)) {
		throw "byte_vector requires all arguments to be in single byte value range";
	}

	std::array<std::byte, sizeof...(Args)> arr{ static_cast<const std::byte>(args)... };
	return std::vector<std::byte>{ arr.begin(), arr.end() };
}

// Casts underlying bytes of a given value to a different type.
// Basically just reinterpret_cast, but with static_assert for size.
template<typename T, typename Val>
[[nodiscard]] const T bytes_cast(const Val& ref) noexcept {
	static_assert(sizeof(Val) >= sizeof(T));
	return *reinterpret_cast<const T*>(&ref);
}

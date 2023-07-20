#pragma once

#include <array>
#include <span>
#include <cmath>
#include <numeric>


namespace stdexp {
	template<typename T, size_t Size = std::numeric_limits<size_t>::max(), typename Val = void>
	constexpr std::span<T, Size> span_cast(const Val& v) {
		static_assert(Size == std::numeric_limits<size_t>::max() || Size * sizeof(T) <= sizeof(Val));
		constexpr size_t n{ std::min(Size, sizeof(Val) / sizeof(T)) };
		
		return std::span<T, Size>(reinterpret_cast<T*>(const_cast<Val*>(&v)), n);
	}

	template<typename T, size_t Size = std::numeric_limits<size_t>::max(), typename Val = void, size_t Input_Size = std::numeric_limits<size_t>::max()>
	constexpr std::span<T, Size> span_cast(std::span<Val, Input_Size>&& v) {
		static_assert(Size * sizeof(T) == Input_Size * sizeof(Val));
		return std::span<T, Size>(reinterpret_cast<T*>(v.data()), Size);
	}

	template<typename T, size_t Size = std::numeric_limits<size_t>::max()>
	constexpr std::span<T, Size> span_cast(std::byte* ptr) {
		return std::span<T, Size>(reinterpret_cast<T*>(ptr), Size);
	}

	template<typename T, size_t Size = std::numeric_limits<size_t>::max()>
	constexpr std::span<T, Size> span_cast(const std::byte* ptr) {
		return span_cast<T, Size>(const_cast<std::byte*>(ptr));
	}

	template<typename T, typename Val = void>
	T value_cast(const Val& ref) {
		return *reinterpret_cast<T*>(const_cast<Val*>(&ref));
	}
} // stdexp
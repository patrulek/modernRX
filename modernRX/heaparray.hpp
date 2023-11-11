#pragma once

/*
* This file contains a class that represents a heap allocated fixed-sized array, that is aligned to a specified value.
* This class is meant to be used as a indirect replacement for std::vector, when the size of the container wont change.
* Elements are not initialized by default.
*/

#include <ranges>

#include "aliases.hpp"

template<typename T, size_t Align = sizeof(T)>
class HeapArray {
    static_assert(Align >= std::hardware_destructive_interference_size, "Alignment cannot be lesser than a single cache line size.");
    static_assert(Align % sizeof(T) == 0, "Alignment must be multiply of element size.");
    static_assert(std::has_single_bit(Align), "Alignment must be power of two.");

public:
    using value_type = T;

    [[nodiscard]] constexpr explicit HeapArray() noexcept = default;
    [[nodiscard]] constexpr explicit HeapArray(const size_t capacity) noexcept {
        reserve(capacity);
    }
    constexpr ~HeapArray() noexcept {
        if (data_ != nullptr) {
            _aligned_free(data_);
            data_ = nullptr;
        }
    }

    void reserve(const size_t capacity) {
        if (data_ != nullptr) {
            return;
        }

        data_ = static_cast<T*>(_aligned_malloc(sizeof(T) * capacity, Align));
        capacity_ = capacity;
    }

    constexpr size_t size() const noexcept {
        return size_;
    }
    
    template<std::ranges::contiguous_range Rng>
    constexpr void append_range(Rng&& range) {
        std::memcpy(data_ + size_, range.data(), range.size() * sizeof(T));
        size_ += range.size();
    }

    constexpr void clear() {
        size_ = 0;
    }

    constexpr T back() const noexcept {
        return data_[size_ - 1];
    }

    constexpr auto begin() noexcept {
        return std::span<T>(data_, size_).begin();
    }

    constexpr auto begin() const noexcept {
        return const_span<T>(data_, size_).begin();
    }

    constexpr auto end() noexcept {
        return std::span<T>(data_, size_).end();
    }

    constexpr auto end() const noexcept {
        return const_span<T>(data_, size_).end();
    }

    [[nodiscard]] constexpr explicit HeapArray(const HeapArray&) = delete;
    constexpr HeapArray& operator=(const HeapArray&) = delete;
    [[nodiscard]] constexpr HeapArray(HeapArray&& other) noexcept {
        this->~HeapArray();
        data_ = other.data_;
        capacity_ = other.capacity_;
        size_ = other.size_;
        other.data_ = nullptr;
        other.capacity_ = 0;
    }
    constexpr HeapArray& operator=(HeapArray&& other) noexcept {
        this->~HeapArray();
        data_ = other.data_;
        capacity_ = other.capacity_;
        size_ = other.size_;
        other.data_ = nullptr;
        other.capacity_ = 0;
        return *this;
    }

    [[nodiscard]] constexpr const T& operator[](const size_t index) const noexcept {
        return data_[index];
    }

    [[nodiscard]] constexpr T& operator[](const size_t index) noexcept {
        return data_[index];
    }

    [[nodiscard]] constexpr std::span<T> buffer() noexcept {
        return std::span<T>(data_, capacity_);
    }

    [[nodiscard]] constexpr const_span<T> view() const noexcept {
        return const_span<T>(data_, capacity_);
    }

    [[nodiscard]] constexpr std::span<T> buffer(const size_t offset, const size_t size) noexcept {
        return std::span<T>(data_ + offset, size);
    }

    [[nodiscard]] constexpr std::span<T> view(const size_t offset, const size_t size) const noexcept {
        return const_span<T>(data_ + offset, size);
    }

    [[nodiscard]] constexpr const T* data() const noexcept {
        return data_;
    }

    [[nodiscard]] constexpr T* data() noexcept {
        return data_;
    }
private:
    T* data_{ nullptr };
    size_t capacity_{ 0 };
    size_t size_{ 0 };
};

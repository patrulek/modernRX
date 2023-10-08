#pragma once

/*
* This file contains a class that represents a heap allocated fixed-sized array, that is aligned to a specified value.
* This class is meant to be used as a indirect replacement for std::vector, when the size of the container wont change.
* Elements are not initialized by default.
*/

#include "aliases.hpp"

template<typename T, size_t Align = sizeof(T)>
class HeapArray {
    static_assert(Align >= std::hardware_destructive_interference_size, "Alignment cannot be lesser than a single cache line size.");
    static_assert(Align % sizeof(T) == 0, "Alignment must be multiply of element size.");
    static_assert(std::has_single_bit(Align), "Alignment must be power of two.");

public:
    [[nodiscard]] constexpr HeapArray() noexcept 
        : data_(nullptr), size_(0) {}
    [[nodiscard]] constexpr HeapArray(const size_t size) noexcept
        : data_(static_cast<T*>(_aligned_malloc(sizeof(T)* size, Align))), size_(size) {}
    constexpr ~HeapArray() noexcept {
        if (data_ != nullptr) {
            _aligned_free(data_);
            data_ = nullptr;
        }
    }

    HeapArray(const HeapArray&) = delete;
    constexpr HeapArray& operator=(const HeapArray&) = delete;
    [[nodiscard]] HeapArray(HeapArray&& other) noexcept {
        this->~HeapArray();
        data_ = other.data_;
        size_ = other.size_;
        other.data_ = nullptr;
        other.size_ = 0;
    }
    constexpr HeapArray& operator=(HeapArray&& other) noexcept {
        this->~HeapArray();
        data_ = other.data_;
        size_ = other.size_;
        other.data_ = nullptr;
        other.size_ = 0;
        return *this;
    }

    [[nodiscard]] constexpr const T& operator[](const size_t index) const noexcept {
        return data_[index];
    }

    [[nodiscard]] constexpr T& operator[](const size_t index) noexcept {
        return data_[index];
    }

    [[nodiscard]] constexpr std::span<T> buffer() noexcept {
        return std::span<T>(data_, size_);
    }

    [[nodiscard]] constexpr const_span<T> view() const noexcept {
        return const_span<T>(data_, size_);
    }

    [[nodiscard]] constexpr std::span<T> buffer(const size_t offset, const size_t size) noexcept {
        return std::span<T>(data_ + offset, size);
    }

    [[nodiscard]] constexpr std::span<T> view(const size_t offset, const size_t size) const noexcept {
        return const_span<T>(data_ + offset, size);
    }
private:
    T* data_;
    size_t size_;
};

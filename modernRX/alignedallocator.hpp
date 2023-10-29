#pragma once

/*
* Aligned allocator for STL containers. Uses msvc-specific functions.
* Not a part of RandomX algorithm.
*/

#include <bit>
#include <utility>

template<typename T, size_t Alignment = 4096>
struct AlignedAllocator {
public:
    static_assert(Alignment >= sizeof(T), "Alignment must be greater than or equal to sizeof(T)");
    static_assert(std::has_single_bit(Alignment), "Alignment must be a power of 2");

    using value_type = T;
    /**
     * This is only necessary because AlignedAllocator has a second template
     * argument for the alignment that will make the default
     * std::allocator_traits implementation fail during compilation.
     * @see https://stackoverflow.com/a/48062758/2191065
     */
    template<class OtherElementType>
    struct rebind
    {
        using other = AlignedAllocator<OtherElementType, std::max<size_t>(sizeof(OtherElementType), Alignment)>;
    };

    constexpr AlignedAllocator() noexcept = default;

    template<typename U>
    constexpr AlignedAllocator(const AlignedAllocator<U, std::max<size_t>(sizeof(U), Alignment)>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t size) {
        return static_cast<T*>(_aligned_malloc(sizeof(T) * size, Alignment));
    }

    void deallocate(T* data, [[maybe_unused]] std::size_t size_bytes) {
        _aligned_free(data);
    }
};

#pragma once

/*
* JIT Compiler for RandomX's Superscalar programs.
* Compiler uses AVX2 instructions. 
*/

#include <span>
#include <vector>

#include "randomxparams.hpp"
#include "virtualmem.hpp"

namespace modernRX {
    struct SuperscalarProgram;

    // DatasetItem defines single cache and dataset item used during generation. Must be 64-bytes size.
    using DatasetItem = std::array<uint64_t, 8>;
    static_assert(sizeof(DatasetItem) == 64);

    // RCX - submemory span, RDX - cache_ptr, R8 - cache_item_mask, R9 - start_item
    using JITDatasetItemProgram = void(*)(std::span<DatasetItem> submemory, const uint64_t cache_ptr, const uint64_t cache_item_mask, const uint64_t start_item);

    // JIT-compile superscalar programs into a 4-batch DatasetItem generation function.
    // Important to note: 
    //   * prologue includes pushing registers following the x64 Windows calling convention.
    //   * prologue includes aligning stack to 64 bytes.
    //   * prologue initializes some variables on stack and puts immediate values into data section.
    //   * epilogue includes popping registers, unaligning stack, zeroing upper AVX registers bits and returning.
    //   * compilation does apply to dataset item initialization and finalization; it JIT-compiles superscalar programs into whole function.
    //   * expects arguments passed to function: RCX - submemory span, RDX - cache_ptr, R8 - cache_item_mask, R9 - start_item
    // Whole program will look like this:
    // JitProgram(submemory, cache_ptr, cache_item_mask, start_item):  
    //   push registers
    //   align stack to 64 bytes
    //   set data section pointer
    //   store immediate values and initialize stack variables
    //   set dataset pointer and loop counter
    //   start loop over whole submemory:
    //     initialize 4-batch of dataset items
    //     JIT-compile 1st program  (prefetch cache items -> execute all program instructions -> tranpose dataset items to xor with cache_items -> transpose back)
    //     JIT-compile 2nd program
    //     ...
    //     JIT-compile last program ( ... -> ... -> ... -> instead of transposing back, store dataset items to submemory)
    //     update some variables for next iteration
    //     decrease loop counter
    //   destroy stack variables
    //   unalign stack
    //   pop registers
    //   zero upper AVX registers bits
    //   return
    // 
    // After compilation, sets the code buffer as executable and returns a function pointer.
    // May throw.

    [[nodiscard]] jit_function_ptr<JITDatasetItemProgram> compile(const_span<SuperscalarProgram, Rx_Cache_Accesses> programs);
}

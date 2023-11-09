#pragma once

/*
* Single-threaded RandomX VirtualMachine with JIT-compiler:
* https://github.com/tevador/RandomX/blob/master/doc/specs.md#2-algorithm-description
* https://github.com/tevador/RandomX/blob/master/doc/specs.md#4-virtual-machine
* https://github.com/tevador/RandomX/blob/master/doc/specs.md#5-instruction-set
* 
* This is used to generate and execute RandomX programs and return single RandomX hash.
*/

#include <span>

#include "dataset.hpp"
#include "heaparray.hpp"

namespace modernRX {
    // RandomX program JIT-compiled function.
    // ctx - pointer to ProgramContext.
    // scratchpad - pointer to Scratchpad.
    // dataset - pointer to Dataset.
    using JITRxProgram = void(*)(const uintptr_t ctx, const uintptr_t scratchpad, const uintptr_t dataset);

    // Forward declarations.
    struct ProgramContext;
    struct RxInstruction;
    struct RxProgram;

    // Defines RandomX VM bytecode executor.
    class VirtualMachine {
    public:
        // Creates VirtualMachine instance.
        // Seed is overtaken by VirtualMachine and shouldnt be used after creation.
        [[nodiscard]] explicit VirtualMachine(std::span<std::byte, 64> seed, const_span<DatasetItem> dataset);
        
        // Executes chained RandomX programs based on seed provided at creation.
        // Returns result as a 32-bytes hash of final RegisterFile.
        [[nodiscard]] std::array<std::byte, 32> execute();
    private:
        // Generates program based on current seed value.
        [[nodiscard]] std::pair<ProgramContext, RxProgram> generateProgram();

        // JIT-compile RandomX program.
        void compileProgram(const ProgramContext& ctx, const RxProgram& program);

        std::array<std::byte, 64> seed;
        const_span<DatasetItem> dataset;
        HeapArray<std::byte, 4096> scratchpad;
        jit_function_ptr<JITRxProgram> jit{ nullptr };
    };
}

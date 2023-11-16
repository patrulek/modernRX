#pragma once

/*
* Single-threaded RandomX VirtualMachine with JIT-compiler:
* https://github.com/tevador/RandomX/blob/master/doc/specs.md#2-algorithm-description
* https://github.com/tevador/RandomX/blob/master/doc/specs.md#4-virtual-machine
* https://github.com/tevador/RandomX/blob/master/doc/specs.md#5-instruction-set
* 
* This is used to generate and execute RandomX programs and return single RandomX hash.
*/

#include <functional>
#include <span>

#include "blocktemplate.hpp"
#include "bytecodecompiler.hpp"
#include "dataset.hpp"
#include "hash.hpp"
#include "heaparray.hpp"

namespace modernRX {
    // RandomX program JIT-compiled function.
    // ctx - pointer to ProgramContext.
    // scratchpad - pointer to Scratchpad.
    // dataset - pointer to Dataset.
    using JITRxProgram = void(*)(const uintptr_t ctx, const uintptr_t scratchpad, const uintptr_t dataset, const uintptr_t program);

    // Forward declarations.
    struct ProgramContext;
    struct RxInstruction;
    struct RxProgram;

    // Defines RandomX VM bytecode executor.
    class alignas(64) VirtualMachine {
    public:
        [[nodiscard]] explicit VirtualMachine();

        // Resets VirtualMachine with new input and dataset.
        // Another VirtualMachine with same input and dataset will produce same result.
        void reset(BlockTemplate block_template, const_span<DatasetItem> dataset);
        
        // Executes chained RandomX programs based on seed provided at creation.
        // Returns result as a 32-bytes hash of final RegisterFile.
        void execute(std::function<void(const RxHash&)> callback);
    private:
        // Generates program based on current seed value.
        void generateProgram(RxProgram& program);

        // JIT-compile RandomX program.
        void compileProgram(const RxProgram& program);

        std::array<std::byte, 64> seed;
        std::span<const DatasetItem> dataset;
        HeapArray<std::byte, 4096> scratchpad;
        BytecodeCompiler compiler;
        jit_function_ptr<JITRxProgram> jit{ nullptr };
        BlockTemplate block_template;
    };
}

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
    // memory - pointer to Scratchpad.
    // dataset - pointer to Dataset.
    // program - pointer to RandomX program.
    // global - pointer to global, read-only data shared by all VMs.
    using JITRxProgram = void(*)(const uintptr_t scratchpad, const uintptr_t dataset, const uintptr_t program, const uintptr_t global);

    // Forward declarations.
    struct ProgramContext;
    struct RxInstruction;
    struct RxProgram;

    struct PData {
        uint32_t vm_id{ 0 };
        uint32_t hashes{ 0 };
    };

    // Defines RandomX VM bytecode executor.
    class alignas(64) VirtualMachine {
        // Holds representation of register file used by VirtualMachine during program execution.
        // https://github.com/tevador/RandomX/blob/master/doc/specs.md#43-registers
        struct RegisterFile {
            std::array<uint64_t, Int_Register_Count> r{}; // Common integer registers. Source or destination of integer instructions. Can be used as address registers for memory access.
            std::array<intrinsics::xmm128d_t, Float_Register_Count> f{}; // "Additive" registers. Destination of floating point addition and substraction instructions.
            std::array<intrinsics::xmm128d_t, Float_Register_Count> e{}; // "Multiplicative" registers. Destination of floating point multiplication, division and square root instructions.
            std::array<intrinsics::xmm128d_t, Float_Register_Count> a{}; // Read-only, fixed-value floating point registers. Source operand of any floating point instruction.
        };

        static constexpr size_t Required_Memory{ Rx_Scratchpad_L3_Size + sizeof(VirtualMachine::RegisterFile) + Rx_Program_Bytes_Size };

    public:
        [[nodiscard]] explicit VirtualMachine(std::span<std::byte, Required_Memory>, JITRxProgram, const uint32_t = 0);

        // Resets VirtualMachine with new input and dataset.
        // Another VirtualMachine with same input and dataset will produce same result.
        void reset(BlockTemplate block_template, const_span<DatasetItem> dataset) noexcept;
        
        // Executes chained RandomX programs based on seed provided at creation.
        // Returns result as a 32-bytes hash of final RegisterFile.
        void execute(std::function<void(const RxHash&)> callback) noexcept;

        static consteval size_t requiredMemory() noexcept {
            return Required_Memory;
        }

        PData getPData() const noexcept {
            return pdata;
        }
    private:

        // Generates program based on current seed value.
        void generateProgram(RxProgram& program) noexcept;

        // JIT-compile RandomX program.
        void compileProgram(const RxProgram& program) noexcept;

        // Executes chained RandomX programs based on seed provided at creation.
        // Returns result as a 32-bytes hash of final RegisterFile.
        void executeNext(std::function<void(const RxHash&)> callback) noexcept;


        std::array<std::byte, 64> seed;
        BlockTemplate block_template;
        std::span<const DatasetItem> dataset;
        std::span<std::byte, Required_Memory> memory;
        BytecodeCompiler compiler;
        JITRxProgram jit{ nullptr };
        PData pdata;
        alignas(32) RxHash output;
        bool new_block_template{ false };
    };
}

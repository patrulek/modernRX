#pragma once

/*
* Single-threaded RandomX program interpreter: https://github.com/tevador/RandomX/blob/master/doc/specs.md#2-algorithm-description
* https://github.com/tevador/RandomX/blob/master/doc/specs.md#4-virtual-machine
* https://github.com/tevador/RandomX/blob/master/doc/specs.md#5-instruction-set
* This is used to execute RandomX programs and return single RandomX hash.
*/

#include <span>

#include "dataset.hpp"
#include "scratchpad.hpp"
#include "sse.hpp"


namespace modernRX {
    // Forward declarations.
    struct ProgramContext;
    struct RxInstruction;
    struct RxProgram;

    // Defines RandomX VM bytecode interpreter.
    class Interpreter {
    public:
        // Creates interpreter instance.
        // Seed is overtaken by interpreter and shouldnt be used after creation.
        [[nodiscard]] explicit Interpreter(std::span<std::byte, 64> seed, const std::vector<DatasetItem>& dataset);
        
        // Executes chained RandomX programs based on seed provided at creation.
        // Returns result as a 32-bytes hash of final RegisterFile.
        [[nodiscard]] std::array<std::byte, 32> execute();
    private:
        // Generates program based on current seed value.
        [[nodiscard]] std::pair<ProgramContext, RxProgram> generateProgram();

        // Initializes registers before RandomX program execution.
        // Performs steps 1-3 defined by: https://github.com/tevador/RandomX/blob/master/doc/specs.md#462-loop-execution
        void initializeRegisters(ProgramContext& ctx);

        // Generates, executes single RandomX program and returns RegisterFile after execution.
        // Performs step 4 defined by: https://github.com/tevador/RandomX/blob/master/doc/specs.md#462-loop-execution
        void executeProgram(ProgramContext& ctx, const RxProgram& program);

        // Executes all instructions in RandomX program (performs single iteration of program).
        void executeInstruction(ProgramContext& ctx, const RxInstruction& instr);

        // Finalizes registers after RandomX program execution.
        // Performs steps 5-12 defined by: https://github.com/tevador/RandomX/blob/master/doc/specs.md#462-loop-execution
        void finalizeRegisters(ProgramContext& ctx);

        const std::vector<DatasetItem>& dataset;
        std::array<std::byte, 64> seed;
        Scratchpad scratchpad;
    };
}

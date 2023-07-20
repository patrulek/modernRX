#pragma once

#include <span>
#include <vector>

#include "scratchpad.hpp"
#include "sse.hpp"
#include "dataset.hpp"

namespace modernRX::interpreter {
    inline constexpr uint32_t Rx_Program_Size{ 256 };

    // Initialized by AES-filled buffer. Must preserve order of fields.
    struct RxInstruction {
        uint8_t opcode;
        uint8_t dst;
        uint8_t src;
        uint8_t mod;
        uint32_t imm32;

        uint8_t modShift() const {
            return (mod >> 2) % 4;
        }

        uint8_t modMask() const {
            return mod % 4;
        }

        uint8_t modCond() const {
            return mod >> 4;
        }
    };
    static_assert(sizeof(RxInstruction) == 8, "Size of single instruction must be 8 bytes");



    struct RxProgram {
        std::array<uint64_t, 16> entropy{};
        std::array<RxInstruction, Rx_Program_Size> instructions{};
    };

    inline constexpr uint32_t Int_Register_Count{ 8 };
    inline constexpr uint32_t Float_Register_Count{ 4 };

    using I64Reg = uint64_t;
    using F128Reg = intrinsics::sse::xmm<double>;
    struct RegisterFile {
        std::array<I64Reg, Int_Register_Count> r{};
        std::array<F128Reg, Float_Register_Count> f{};
        std::array<F128Reg, Float_Register_Count> e{};
        std::array<F128Reg, Float_Register_Count> a{};
    };

    struct MemoryRegisters {
        uint32_t mx{ 0 };
        uint32_t ma{ 0 };
    };

    struct ProgramConfiguration {
        std::array<uint64_t, 2> e_mask{};
        std::array<uint32_t, 4> read_reg{};
        uint64_t dataset_offset{ 0 };
    };

    struct ProgramContext {
        ProgramContext(const RxProgram& program);

        RegisterFile rf{};
        MemoryRegisters mem{};
        MemoryRegisters sp_addr{};
        ProgramConfiguration cfg{};
        std::array<int32_t, Int_Register_Count> reg_usage{};
        uint32_t ic{ 0 };
        uint32_t iter{ 0 };
        std::array<int16_t, Rx_Program_Size> branch_target{};
    };

    class Interpreter {
    public:
        explicit Interpreter(const DatasetMemory& dataset, const std::span<std::byte, 64> seed);
        RegisterFile execute(const RxProgram& program);
        std::array<std::byte, 32> run();

    private:
        void executeInstruction(ProgramContext& ctx, const RxInstruction& instr);
        void initializeRegisters(ProgramContext& ctx);
        void finalizeRegisters(ProgramContext& ctx);
        RxProgram generateProgram();

        const DatasetMemory& dataset;
        Scratchpad scratchpad;
        std::array<std::byte, 64> seed;
    };


    //void interpreterHash(const std::span<std::byte> input, std::span<std::byte, Rx_Hash_Size> hash) {
    //    std::array<std::byte, 64> seed;
    //    blake2b::hash(seed, input, std::span<std::byte>());
    //    Scratchpad scratchpad{ seed };
    //    intrinsics::resetFloatEnv();

    //    for (auto i{ 0 }; i < Rx_Program_Count - 1; i++) {
    //        auto prog = generateProgram(seed);
    //        interpret(prog, scratchpad);
    //    }

    //    auto prog = generateProgram(seed);
    //    interpret(prog, scratchpad);

    //    RegisterFile rf{};

    //    // get final result
    //    // aes::hash1R(seed, scratchpad); // reg.a
    //    // blake2b::hash(seed, stdexp::span_cast<std::byte>(rf.a), std::span<std::byte>());
    //    // std::memcpy(hash.data(), seed.data(), Rx_Hash_Size);
    //}
}
#pragma once

/*
* RandomX VirtualMachine's bytecode JIT-compiler.
* Defines code generation functions for all RandomX instructions.
*/

#include "bytecode.hpp"

namespace modernRX {
    inline constexpr uint32_t Float_Register_Count{ 4 };
    inline constexpr uint32_t Int_Register_Count{ 8 };

    // Defines single RandomX program instruction: https://github.com/tevador/RandomX/blob/master/doc/specs.md#51-instruction-encoding
    // Initialized by AES-filled buffer. 
    // Must preserve order of fields.
    struct RxInstruction {
        uint8_t opcode; // https://github.com/tevador/RandomX/blob/master/doc/specs.md#511-opcode
        uint8_t dst_register; // https://github.com/tevador/RandomX/blob/master/doc/specs.md#512-dst
        uint8_t src_register; // https://github.com/tevador/RandomX/blob/master/doc/specs.md#513-src
        uint8_t mod; // https://github.com/tevador/RandomX/blob/master/doc/specs.md#514-mod
        uint32_t imm32; // https://github.com/tevador/RandomX/blob/master/doc/specs.md#515-imm32

        // Used to select between Scratchpad levels L1 and L2 when reading from or writing to memory.
        [[nodiscard]] uint8_t modMask() const noexcept {
            return mod % 4;
        }

        // Used by IADD_RS instruction.
        [[nodiscard]] uint8_t modShift() const noexcept {
            return (mod >> 2) % 4;
        }

        // Used by CBRANCH and ISTORE instructions.
        [[nodiscard]] uint8_t modCond() const noexcept {
            return mod >> 4;
        }
    };
    static_assert(sizeof(RxInstruction) == 8, "Size of single instruction must be 8 bytes");

    struct alignas(32) BytecodeCompiler {
        std::array<int32_t, Int_Register_Count> reg_usage{ -1, -1, -1, -1, -1, -1, -1, -1 };
        char* code_buffer{ nullptr };
        int32_t code_size{ 0 };
        std::array<int32_t, Rx_Program_Size> instr_offset{};

        void reset() noexcept {
            code_size = 0;
            reg_usage.fill(-1);
            // instr_offset will be overwritten during compilation
        }

        void iaddrs_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void iaddm_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void isubr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void isubm_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void imulr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void imulm_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void imulhr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void imulhm_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void ismulhr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void ismulhm_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void inegr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void ixorr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void ixorm_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void irorr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void irolr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void imulrcp_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void iswapr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void cbranch_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void fswapr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void faddr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void faddm_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void fsubr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void fsubm_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void fscalr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void fmulr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void fdivm_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void fsqrtr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void cfround_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
        void istore_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept;
    };

    using InstrCmpl = void(BytecodeCompiler::*)(const RxInstruction&, const uint32_t);

    // Holds compilation function (code generator) for all possible instruction opcodes.
    inline constexpr std::array<InstrCmpl, LUT_Opcode.size()> LUT_Instr_Cmpl = []() consteval {
        std::array<InstrCmpl, LUT_Opcode.size()> LUT_Instr_Cmpl_{};

        for (uint32_t i = 0; i < LUT_Instr_Cmpl_.size(); ++i) {
            switch (LUT_Opcode[i]) { using enum Bytecode;
            case IADD_RS:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::iaddrs_cmpl;
                break;
            case IADD_M:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::iaddm_cmpl;
                break;
            case ISUB_R:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::isubr_cmpl;
                break;
            case ISUB_M:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::isubm_cmpl;
                break;
            case IMUL_R:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::imulr_cmpl;
                break;
            case IMUL_M:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::imulm_cmpl;
                break;
            case IMULH_R:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::imulhr_cmpl;
                break;
            case IMULH_M:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::imulhm_cmpl;
                break;
            case ISMULH_R:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::ismulhr_cmpl;
                break;
            case ISMULH_M:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::ismulhm_cmpl;
                break;
            case IMUL_RCP:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::imulrcp_cmpl;
                break;
            case INEG_R:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::inegr_cmpl;
                break;
            case IXOR_R:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::ixorr_cmpl;
                break;
            case IXOR_M:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::ixorm_cmpl;
                break;
            case IROR_R:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::irorr_cmpl;
                break;
            case IROL_R:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::irolr_cmpl;
                break;
            case ISWAP_R:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::iswapr_cmpl;
                break;
            case CBRANCH:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::cbranch_cmpl;
                break;
            case FSWAP_R:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::fswapr_cmpl;
                break;
            case FADD_R:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::faddr_cmpl;
                break;
            case FADD_M:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::faddm_cmpl;
                break;
            case FSUB_R:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::fsubr_cmpl;
                break;
            case FSUB_M:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::fsubm_cmpl;
                break;
            case FSCAL_R:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::fscalr_cmpl;
                break;
            case FMUL_R:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::fmulr_cmpl;
                break;
            case FDIV_M:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::fdivm_cmpl;
                break;
            case FSQRT_R:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::fsqrtr_cmpl;
                break;
            case CFROUND:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::cfround_cmpl;
                break;
            case ISTORE:
                LUT_Instr_Cmpl_[i] = &BytecodeCompiler::istore_cmpl;
                break;
            default:
                throw "invalid instruction";
            }
        }

        return LUT_Instr_Cmpl_;
    }();
}

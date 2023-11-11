#pragma once

/*
* RandomX VirtualMachine's bytecode JIT-compiler.
* Defines code generation functions for all RandomX instructions.
*/

#include "assembler.hpp"
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

    void iaddrs_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void iaddm_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void isubr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void isubm_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void imulr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void imulm_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void imulhr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void imulhm_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void ismulhr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void ismulhm_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void inegr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void ixorr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void ixorm_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void irorr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void irolr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void imulrcp_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void iswapr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void cbranch_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void fswapr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void faddr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void faddm_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void fsubr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void fsubm_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void fscalr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void fmulr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void fdivm_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void fsqrtr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void cfround_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);
    void istore_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx);

    using InstrCmpl = decltype(&iaddrs_cmpl);

    // Holds compilation function (code generator) for all possible instruction opcodes.
    inline constexpr std::array<InstrCmpl, LUT_Opcode.size()> LUT_Instr_Cmpl = []() consteval {
        std::array<InstrCmpl, LUT_Opcode.size()> LUT_Instr_Cmpl_{};

        for (uint32_t i = 0; i < LUT_Instr_Cmpl_.size(); ++i) {
            switch (LUT_Opcode[i]) { using enum Bytecode;
            case IADD_RS:
                LUT_Instr_Cmpl_[i] = iaddrs_cmpl;
                break;
            case IADD_M:
                LUT_Instr_Cmpl_[i] = iaddm_cmpl;
                break;
            case ISUB_R:
                LUT_Instr_Cmpl_[i] = isubr_cmpl;
                break;
            case ISUB_M:
                LUT_Instr_Cmpl_[i] = isubm_cmpl;
                break;
            case IMUL_R:
                LUT_Instr_Cmpl_[i] = imulr_cmpl;
                break;
            case IMUL_M:
                LUT_Instr_Cmpl_[i] = imulm_cmpl;
                break;
            case IMULH_R:
                LUT_Instr_Cmpl_[i] = imulhr_cmpl;
                break;
            case IMULH_M:
                LUT_Instr_Cmpl_[i] = imulhm_cmpl;
                break;
            case ISMULH_R:
                LUT_Instr_Cmpl_[i] = ismulhr_cmpl;
                break;
            case ISMULH_M:
                LUT_Instr_Cmpl_[i] = ismulhm_cmpl;
                break;
            case IMUL_RCP:
                LUT_Instr_Cmpl_[i] = imulrcp_cmpl;
                break;
            case INEG_R:
                LUT_Instr_Cmpl_[i] = inegr_cmpl;
                break;
            case IXOR_R:
                LUT_Instr_Cmpl_[i] = ixorr_cmpl;
                break;
            case IXOR_M:
                LUT_Instr_Cmpl_[i] = ixorm_cmpl;
                break;
            case IROR_R:
                LUT_Instr_Cmpl_[i] = irorr_cmpl;
                break;
            case IROL_R:
                LUT_Instr_Cmpl_[i] = irolr_cmpl;
                break;
            case ISWAP_R:
                LUT_Instr_Cmpl_[i] = iswapr_cmpl;
                break;
            case CBRANCH:
                LUT_Instr_Cmpl_[i] = cbranch_cmpl;
                break;
            case FSWAP_R:
                LUT_Instr_Cmpl_[i] = fswapr_cmpl;
                break;
            case FADD_R:
                LUT_Instr_Cmpl_[i] = faddr_cmpl;
                break;
            case FADD_M:
                LUT_Instr_Cmpl_[i] = faddm_cmpl;
                break;
            case FSUB_R:
                LUT_Instr_Cmpl_[i] = fsubr_cmpl;
                break;
            case FSUB_M:
                LUT_Instr_Cmpl_[i] = fsubm_cmpl;
                break;
            case FSCAL_R:
                LUT_Instr_Cmpl_[i] = fscalr_cmpl;
                break;
            case FMUL_R:
                LUT_Instr_Cmpl_[i] = fmulr_cmpl;
                break;
            case FDIV_M:
                LUT_Instr_Cmpl_[i] = fdivm_cmpl;
                break;
            case FSQRT_R:
                LUT_Instr_Cmpl_[i] = fsqrtr_cmpl;
                break;
            case CFROUND:
                LUT_Instr_Cmpl_[i] = cfround_cmpl;
                break;
            case ISTORE:
                LUT_Instr_Cmpl_[i] = istore_cmpl;
                break;
            default:
                throw "invalid instruction";
            }
        }

        return LUT_Instr_Cmpl_;
    }();
}
#pragma once

/*
* RandomX VirtualMachine's bytecode JIT-compiler.
* Defines code generation functions for all RandomX instructions.
*/

#include <print>

#include "bytecode.hpp"
#include "exception.hpp"

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

    // Holds RandomX program entropy and instructions: https://github.com/tevador/RandomX/blob/master/doc/specs.md#44-program-buffer
    // Initialized by AES-filled buffer. Must preserve order of fields.
    struct RxProgram {
        std::array<uint64_t, 16> entropy{};
        std::array<RxInstruction, Rx_Program_Size> instructions{};
    };
    static_assert(sizeof(RxProgram) == Rx_Program_Bytes_Size); // Size of random program is also used in different context. Make sure both values match.
    static_assert(offsetof(RxProgram, entropy) == 0);

    template<typename Tout, typename Tin>
    inline Tout ForceCast(const Tin in)
    {
        union
        {
            Tin in;
            Tout out;
        } u = { in };

        return u.out;
    };

    struct alignas(64) BytecodeCompiler {
        std::array<int16_t, Rx_Program_Size> instr_offset{};
        std::array<int16_t, Int_Register_Count> reg_usage{ -1, -1, -1, -1, -1, -1, -1, -1 };
        const int64_t Base_Cmpl_Addr{ ForceCast<int64_t>(&BytecodeCompiler::irorr_cmpl) };
        char* code_buffer{ nullptr };
        RxProgram* program{ nullptr };
        int16_t code_size{ 0 };
        int16_t last_fp_instr{ -1 };
        int16_t last_cfround_instr{ -1 };

        void reset() noexcept {
            code_size = 0;
            reg_usage.fill(-1);
            last_fp_instr = -1;
            last_cfround_instr = -1;
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

    inline int16_t bcCmplDiff(InstrCmpl base, InstrCmpl func) {
        const auto dist{ ForceCast<int32_t>(func) - ForceCast<int32_t>(base) };
        if (dist > std::numeric_limits<int16_t>::max() || dist < std::numeric_limits<int16_t>::min()) {
            throw Exception(std::format("Distance between functions out of bounds; should be true {} <= {} <= {}", 
                std::numeric_limits<int16_t>::min(), dist, std::numeric_limits<int16_t>::max()));
        }

        return static_cast<int16_t>(dist);
    }

    inline const alignas(64) std::array<int16_t, LUT_Opcode.size()> LUT_Instr_Cmpl_Offsets = []() {
        std::array<int16_t, LUT_Opcode.size()> LUT_Instr_Cmpl_Offsets_{};
        const auto Base_Cmpl_Func{ ForceCast<InstrCmpl>(&BytecodeCompiler::irorr_cmpl) };

        try {
            for (uint32_t i = 0; i < LUT_Instr_Cmpl_Offsets_.size(); ++i) {
                switch (LUT_Opcode[i]) { using enum Bytecode;
                case IADD_RS:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::iaddrs_cmpl);
                    break;
                case IADD_M:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::iaddm_cmpl);
                    break;
                case ISUB_R:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::isubr_cmpl);
                    break;
                case ISUB_M:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::isubm_cmpl);
                    break;
                case IMUL_R:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::imulr_cmpl);
                    break;
                case IMUL_M:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::imulm_cmpl);
                    break;
                case IMULH_R:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::imulhr_cmpl);
                    break;
                case IMULH_M:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::imulhm_cmpl);
                    break;
                case ISMULH_R:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::ismulhr_cmpl);
                    break;
                case ISMULH_M:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::ismulhm_cmpl);
                    break;
                case IMUL_RCP:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::imulrcp_cmpl);
                    break;
                case INEG_R:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::inegr_cmpl);
                    break;
                case IXOR_R:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::ixorr_cmpl);
                    break;
                case IXOR_M:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::ixorm_cmpl);
                    break;
                case IROR_R:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::irorr_cmpl);
                    break;
                case IROL_R:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::irolr_cmpl);
                    break;
                case ISWAP_R:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::iswapr_cmpl);
                    break;
                case CBRANCH:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::cbranch_cmpl);
                    break;
                case FSWAP_R:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::fswapr_cmpl);
                    break;
                case FADD_R:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::faddr_cmpl);
                    break;
                case FADD_M:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::faddm_cmpl);
                    break;
                case FSUB_R:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::fsubr_cmpl);
                    break;
                case FSUB_M:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::fsubm_cmpl);
                    break;
                case FSCAL_R:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::fscalr_cmpl);
                    break;
                case FMUL_R:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::fmulr_cmpl);
                    break;
                case FDIV_M:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::fdivm_cmpl);
                    break;
                case FSQRT_R:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::fsqrtr_cmpl);
                    break;
                case CFROUND:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::cfround_cmpl);
                    break;
                case ISTORE:
                    LUT_Instr_Cmpl_Offsets_[i] = bcCmplDiff(Base_Cmpl_Func, &BytecodeCompiler::istore_cmpl);
                    break;
                }
            }
        } catch (const std::exception& ex) {
            std::println("Failed to initialize LUT_Instr_Cmpl_Offsets: {}", ex.what());
            std::terminate();
        }

        return LUT_Instr_Cmpl_Offsets_;
    }();
}

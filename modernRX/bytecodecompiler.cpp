#pragma once

#include "bytecodecompiler.hpp"
#include "randomxparams.hpp"
#include "reciprocal.hpp"
#include "sse.hpp"

namespace modernRX {
    namespace {
        constexpr uint32_t Scratchpad_L1_Mask{ (Rx_Scratchpad_L1_Size - 1) & ~7 }; // L1 cache 8-byte alignment mask.
        constexpr uint32_t Scratchpad_L2_Mask{ (Rx_Scratchpad_L2_Size - 1) & ~7 }; // L2 cache 8-byte alignment mask.
        constexpr uint32_t Scratchpad_L3_Mask{ (Rx_Scratchpad_L3_Size - 1) & ~7 }; // L3 cache 8-byte alignment mask.

        void setJmpPoint(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const uint32_t reg_idx, const uint32_t instr_idx);
    }

    using namespace assembler;
    using namespace assembler::registers;

    void iaddrs_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        constexpr uint8_t Displacement_Reg_Idx{ 5 };
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto src_register{ instr.src_register % Int_Register_Count };
        const bool with_imm{ dst_register == Displacement_Reg_Idx };
        const auto offset = with_imm ? instr.imm32 : 0;
        const auto asmb_dst_reg{ Register::GPR(dst_register | 8) };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };

        asmb.lea(asmb_dst_reg, asmb_dst_reg[asmb_src_reg[offset]], 1 << instr.modShift());
        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void iaddm_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto asmb_dst_reg{ Register::GPR(dst_register | 8) };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };
        auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

        if (dst_register == src_register) {
            const auto offset{ instr.imm32 & Scratchpad_L3_Mask };
            asmb.add(asmb_dst_reg, RSI[offset]);
        } else {
            asmb.lea(RAX, asmb_src_reg[instr.imm32]);
            asmb.and_(RAX, mem_mask);
            asmb.add(asmb_dst_reg, RSI[RAX[0]]);
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void isubr_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto asmb_dst_reg{ Register::GPR(dst_register | 8) };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };

        if (dst_register == src_register) {
            asmb.sub(asmb_dst_reg, static_cast<int32_t>(instr.imm32));
        } else {
            asmb.sub(asmb_dst_reg, asmb_src_reg);
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void isubm_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto asmb_dst_reg{ Register::GPR(dst_register | 8) };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };
        auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

        if (dst_register == src_register) {
            const auto offset{ instr.imm32 & Scratchpad_L3_Mask };
            asmb.sub(asmb_dst_reg, RSI[offset]);
        } else {
            asmb.lea(RAX, asmb_src_reg[instr.imm32]);
            asmb.and_(RAX, mem_mask);
            asmb.sub(asmb_dst_reg, RSI[RAX[0]]);
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void imulr_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto asmb_dst_reg{ Register::GPR(dst_register | 8) };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };

        if (dst_register == src_register) {
            asmb.imul(asmb_dst_reg, static_cast<int32_t>(instr.imm32));
        } else {
            asmb.imul(asmb_dst_reg, asmb_src_reg);
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void imulm_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto asmb_dst_reg{ Register::GPR(dst_register | 8) };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };
        auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

        if (dst_register == src_register) {
            const auto offset{ instr.imm32 & Scratchpad_L3_Mask };
            asmb.imul(asmb_dst_reg, RSI[offset]);
        } else {
            asmb.lea(RCX, asmb_src_reg[instr.imm32]);
            asmb.and_(RCX, mem_mask);
            asmb.imul(asmb_dst_reg, RSI[RCX[0]]);
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void imulhr_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto asmb_dst_reg{ Register::GPR(dst_register | 8) };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };
        asmb.mulh(asmb_dst_reg, asmb_src_reg);

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void imulhm_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto asmb_dst_reg{ Register::GPR(dst_register | 8) };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };
        auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

        if (dst_register == src_register) {
            const auto offset{ instr.imm32 & Scratchpad_L3_Mask };
            asmb.mulh(asmb_dst_reg, RSI[offset]);
        } else {
            asmb.lea(RCX, asmb_src_reg[instr.imm32]);
            asmb.and_(RCX, mem_mask);
            asmb.mulh(asmb_dst_reg, RSI[RCX[0]]);
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void ismulhr_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto asmb_dst_reg{ Register::GPR(dst_register | 8) };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };
        asmb.imulh(asmb_dst_reg, asmb_src_reg);

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void ismulhm_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto asmb_dst_reg{ Register::GPR(dst_register | 8) };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };
        auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

        if (dst_register == src_register) {
            const auto offset{ instr.imm32 & Scratchpad_L3_Mask };
            asmb.imulh(asmb_dst_reg, RSI[offset]);
        } else {
            asmb.lea(RCX, asmb_src_reg[instr.imm32]);
            asmb.and_(RCX, mem_mask);
            asmb.imulh(asmb_dst_reg, RSI[RCX[0]]);
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void inegr_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto asmb_dst_reg{ Register::GPR(dst_register | 8) };
        asmb.neg(asmb_dst_reg);

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void ixorr_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto asmb_dst_reg{ Register::GPR(dst_register | 8) };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };

        if (dst_register == src_register) {
            asmb.xor_(asmb_dst_reg, static_cast<int32_t>(instr.imm32));
        } else {
            asmb.xor_(asmb_dst_reg, asmb_src_reg);
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void ixorm_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto asmb_dst_reg{ Register::GPR(dst_register | 8) };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };
        auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

        if (dst_register == src_register) {
            const auto offset{ instr.imm32 & Scratchpad_L3_Mask };
            asmb.xor_(asmb_dst_reg, RSI[offset]);
        } else {
            asmb.lea(RAX, asmb_src_reg[instr.imm32]);
            asmb.and_(RAX, mem_mask);
            asmb.xor_(asmb_dst_reg, RSI[RAX[0]]);
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void irorr_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto asmb_dst_reg{ Register::GPR(dst_register | 8) };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };

        if (dst_register == src_register) {
            asmb.ror(asmb_dst_reg, instr.imm32 % 64);
        } else {
            asmb.ror(asmb_dst_reg, asmb_src_reg);
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void irolr_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto asmb_dst_reg{ Register::GPR(dst_register | 8) };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };

        if (dst_register == src_register) {
            asmb.rol(asmb_dst_reg, instr.imm32 % 64);
        } else {
            asmb.rol(asmb_dst_reg, asmb_src_reg);
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void imulrcp_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto asmb_dst_reg{ Register::GPR(dst_register | 8) };

        if (instr.imm32 != 0 && !std::has_single_bit(instr.imm32)) {
            asmb.mov(RAX, reciprocal(instr.imm32));
            asmb.imul(asmb_dst_reg, RAX);
            setJmpPoint(asmb, reg_usage, dst_register, idx);
        }
    }

    void iswapr_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto asmb_dst_reg{ Register::GPR(dst_register | 8) };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };

        if (src_register != dst_register) {
            asmb.xchg(asmb_dst_reg, asmb_src_reg);
            setJmpPoint(asmb, reg_usage, src_register, idx);
            setJmpPoint(asmb, reg_usage, dst_register, idx);
        }
    }

    void cbranch_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto asmb_dst_reg{ Register::GPR(dst_register | 8) };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };

        constexpr uint32_t Condition_Mask{ (1 << Rx_Jump_Bits) - 1 };
        const auto shift{ instr.modCond() + Rx_Jump_Offset };
        const auto mem_mask{ Condition_Mask << shift };

        static_assert(Rx_Jump_Offset > 0, "Below simplification requires this assertion");
        uint64_t imm{ static_cast<int32_t>(instr.imm32) | (1ULL << shift) };
        imm &= ~(1ULL << (shift - 1)); // Clear the bit below the condition mask - this limits the number of successive jumps to 2.

        const auto jmp_target{ reg_usage[dst_register] + 1 };

        asmb.add(asmb_dst_reg, static_cast<int32_t>(imm));
        asmb.test(asmb_dst_reg, mem_mask);
        asmb.jz(jmp_target);
        setJmpPoint(asmb, reg_usage, dst_register, idx);

        for (auto& reg : reg_usage) reg = idx; // Set all registers as used.
    }

    void fswapr_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };
        const auto f_asmb_dst_reg{ Register::XMM(f_dst_register) };
        const auto e_asmb_dst_reg{ Register::XMM(f_dst_register + 4) };

        const auto& swapreg{ dst_register < Float_Register_Count ? f_asmb_dst_reg : e_asmb_dst_reg };
        asmb.vshufpd(swapreg, swapreg, swapreg, 0b0000'0001);
    }

    void faddr_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };
        const auto f_src_register{ instr.src_register % Float_Register_Count };
        const auto f_asmb_dst_reg{ Register::XMM(f_dst_register) };
        const auto a_asmb_src_reg{ Register::XMM(f_src_register + 8) };

        asmb.vaddpd(f_asmb_dst_reg, a_asmb_src_reg);
    }

    void faddm_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };
        const auto f_asmb_dst_reg{ Register::XMM(f_dst_register) };
        auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

        asmb.lea(RAX, asmb_src_reg[instr.imm32]);
        asmb.and_(RAX, mem_mask);
        asmb.vcvtdq2pd(XMM12, RSI[RAX[0]]);
        asmb.vaddpd(f_asmb_dst_reg, XMM12);
    }

    void fsubr_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };
        const auto f_src_register{ instr.src_register % Float_Register_Count };
        const auto f_asmb_dst_reg{ Register::XMM(f_dst_register) };
        const auto a_asmb_src_reg{ Register::XMM(f_src_register + 8) };

        asmb.vsubpd(f_asmb_dst_reg, a_asmb_src_reg);
    }

    void fsubm_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };
        const auto f_asmb_dst_reg{ Register::XMM(f_dst_register) };
        auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

        asmb.lea(RAX, asmb_src_reg[instr.imm32]);
        asmb.and_(RAX, mem_mask);
        asmb.vcvtdq2pd(XMM12, RSI[RAX[0]]);
        asmb.vsubpd(f_asmb_dst_reg, XMM12);
    }

    void fscalr_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };
        const auto f_asmb_dst_reg{ Register::XMM(f_dst_register) };

        asmb.vpxor(f_asmb_dst_reg, f_asmb_dst_reg, XMM14);
    }

    void fmulr_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };
        const auto f_src_register{ instr.src_register % Float_Register_Count };
        const auto e_asmb_dst_reg{ Register::XMM(f_dst_register + 4) };
        const auto a_asmb_src_reg{ Register::XMM(f_src_register + 8) };

        asmb.vmulpd(e_asmb_dst_reg, a_asmb_src_reg);
    }

    void fdivm_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };
        const auto e_asmb_dst_reg{ Register::XMM(f_dst_register + 4) };
        auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

        asmb.vmovdqu(XMM13, RDI[280 + 0]); // Load e_mask.
        asmb.lea(RAX, asmb_src_reg[instr.imm32]);
        asmb.and_(RAX, mem_mask);
        asmb.vcvtdq2pd(XMM12, RSI[RAX[0]]);
        asmb.vpand(XMM12, XMM12, XMM15);
        asmb.vpor(XMM12, XMM12, XMM13);
        asmb.vdivpd(e_asmb_dst_reg, XMM12);
    }

    void fsqrtr_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };
        const auto e_asmb_dst_reg{ Register::XMM(f_dst_register + 4) };

        asmb.vsqrtpd(e_asmb_dst_reg);
    }

    void cfround_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };

        asmb.mov(RAX, asmb_src_reg);
        asmb.ror(RAX, static_cast<int32_t>(instr.imm32 % 64));
        asmb.and_(RAX, 3);
        asmb.rol(RAX, 13);
        asmb.or_(RAX, intrinsics::sse::Rx_Mxcsr_Default);
        asmb.push(RAX);
        asmb.ldmxcsr(RSP[0]);
        asmb.pop(RAX);
    }

    void istore_cmpl(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto asmb_dst_reg{ Register::GPR(dst_register | 8) };
        const auto asmb_src_reg{ Register::GPR(src_register | 8) };
        auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

        constexpr uint32_t L3_Store_Condition{ 14 };

        if (instr.modCond() >= L3_Store_Condition) {
            mem_mask = Scratchpad_L3_Mask;
        }

        asmb.lea(RAX, asmb_dst_reg[static_cast<int32_t>(instr.imm32)]);
        asmb.and_(RAX, mem_mask);
        asmb.mov(RSI[RAX[0]], asmb_src_reg);
    }

    namespace {
        void setJmpPoint(assembler::Context& asmb, std::span<int32_t, Int_Register_Count> reg_usage, const uint32_t reg_idx, const uint32_t instr_idx) {
            reg_usage[reg_idx] = instr_idx;
            asmb.label(instr_idx + 1, false);
        };
    }
}

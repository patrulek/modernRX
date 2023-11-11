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

        void setJmpPoint(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const uint32_t reg_idx, const uint32_t instr_idx);

        template<int Byte, typename Val>
        [[nodiscard]] constexpr int8_t b(const Val& v) noexcept {
            static_assert(Byte < sizeof(Val));
            return *(reinterpret_cast<const int8_t*>(&v) + Byte);
        }
    }

    using namespace assembler;
    using namespace assembler::registers;

    void iaddrs_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        constexpr uint8_t Displacement_Reg_Idx{ 5 };
        const uint8_t dst_register{ instr.dst_register % Int_Register_Count };
        const uint8_t src_register{ instr.src_register % Int_Register_Count };
        
        const auto scale{ 64 * instr.modShift() };
        const auto index{ 8 * src_register };
        const auto base{ dst_register };

        if (dst_register == Displacement_Reg_Idx) {
            const auto imm{ instr.imm32 };
            asmb.inject(char_array(
                /* LEA */ 0x4f, 0x8d, 0xac, scale | index | base, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm)
            ));
        } else {
            asmb.inject(char_array(
                /* LEA */ 0x4f, 0x8d, 0x04 + 8 * dst_register, scale | index | base
            ));
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void iaddm_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Int_Register_Count };
        const uint8_t src_register{ instr.src_register % Int_Register_Count };

        if (dst_register == src_register) {
            const auto offset{ instr.imm32 & Scratchpad_L3_Mask };
            asmb.inject(char_array(
                /* ADD */ 0x4c, 0x03, 0x86 + 8 * dst_register, b<0>(offset), b<1>(offset), b<2>(offset), b<3>(offset)
            ));
        } else {
            const auto imm{ instr.imm32 };
            const auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

            if (src_register != RSP.idx) {
                asmb.inject(char_array(
                    /* LEA */ 0x49, 0x8d, 0x80 + src_register, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
                    /* AND */ 0x48, 0x25, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
                    /* ADD */ 0x4c, 0x03, 0x04 + 8 * dst_register, 0x06
                ));
            } else {
                asmb.inject(char_array(
                    /* LEA */ 0x49, 0x8d, 0x80 + src_register, 0x24, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
                    /* AND */ 0x48, 0x25, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
                    /* ADD */ 0x4c, 0x03, 0x04 + 8 * dst_register, 0x06
                ));
            }
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void isubr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Int_Register_Count };
        const uint8_t src_register{ instr.src_register % Int_Register_Count };

        if (dst_register == src_register) {
            const auto imm{ static_cast<int32_t>(instr.imm32) };
            asmb.inject(char_array(
                /* SUB */ 0x49, 0x81, 0xe8 + dst_register, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm)
            ));
        } else {
            asmb.inject(char_array(
                /* SUB */ 0x4d, 0x29, 0xc0 + dst_register + 8 * src_register
            ));
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void isubm_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Int_Register_Count };
        const uint8_t src_register{ instr.src_register % Int_Register_Count };

        if (dst_register == src_register) {
            const auto offset{ instr.imm32 & Scratchpad_L3_Mask };
            asmb.inject(char_array(
                /* SUB */ 0x4c, 0x2b, 0x86 + 8 * dst_register, b<0>(offset), b<1>(offset), b<2>(offset), b<3>(offset)
            ));
        } else {
            const auto imm{ instr.imm32 };
            const auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

            if (src_register != RSP.idx) {
                asmb.inject(char_array(
                    /* LEA */ 0x49, 0x8d, 0x80 + src_register, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
                    /* AND */ 0x48, 0x25, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
                    /* SUB */ 0x4c, 0x2b, 0x04 + 8 * dst_register, 0x06
                ));
            } else {
                asmb.inject(char_array(
                    /* LEA */ 0x49, 0x8d, 0x80 + src_register, 0x24, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
                    /* AND */ 0x48, 0x25, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
                    /* SUB */ 0x4c, 0x2b, 0x04 + 8 * dst_register, 0x06
                ));
            }
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void imulr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Int_Register_Count };
        const uint8_t src_register{ instr.src_register % Int_Register_Count };

        if (dst_register == src_register) {
            const auto imm{ static_cast<int32_t>(instr.imm32) };
            asmb.inject(char_array(
                /* IMUL */ 0x4d, 0x69, 0xc0 + 9 * dst_register, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm)
            ));
        } else {
            asmb.inject(char_array(
                /* IMUL */ 0x4d, 0x0f, 0xaf, 0xc0 + 8 * dst_register + src_register
            ));
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void imulm_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Int_Register_Count };
        const uint8_t src_register{ instr.src_register % Int_Register_Count };

        if (dst_register == src_register) {
            const auto offset{ instr.imm32 & Scratchpad_L3_Mask };
            asmb.inject(char_array(
                /* IMUL */ 0x4c, 0x0f, 0xaf, 0x86 + 8 * dst_register, b<0>(offset), b<1>(offset), b<2>(offset), b<3>(offset)
            ));
        } else {
            const auto imm{ instr.imm32 };
            const auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

            if (src_register != RSP.idx) {
                asmb.inject(char_array(
                    /* LEA */ 0x49, 0x8d, 0x88 + src_register, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
                    /* AND */ 0x48, 0x81, 0xe1, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
                    /* IMUL */ 0x4c, 0x0f, 0xaf, 0x04 + 8 * dst_register, 0x0e
                ));
            } else {
                asmb.inject(char_array(
                    /* LEA */ 0x49, 0x8d, 0x88 + src_register, 0x24, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
                    /* AND */ 0x48, 0x81, 0xe1, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
                    /* IMUL */ 0x4c, 0x0f, 0xaf, 0x04 + 8 * dst_register, 0x0e
                ));
            }
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void imulhr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Int_Register_Count };
        const uint8_t src_register{ instr.src_register % Int_Register_Count };

        asmb.inject(char_array(
            /* MOV */ 0x4c, 0x89, 0xc0 + 8 * dst_register,
            /* MULH */ 0x49, 0xf7, 0xe0 + src_register,
            /* MOV */ 0x49, 0x89, 0xd0 + dst_register
        ));

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void imulhm_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Int_Register_Count };
        const uint8_t src_register{ instr.src_register % Int_Register_Count };

        if (dst_register == src_register) {
            const auto offset{ instr.imm32 & Scratchpad_L3_Mask };
            asmb.inject(char_array(
                /* MOV */ 0x4c, 0x89, 0xc0 + 8 * dst_register, 
                /* MULH */ 0x48, 0xf7, 0xa6, b<0>(offset), b<1>(offset), b<2>(offset), b<3>(offset),
                /* MOV */ 0x49, 0x89, 0xd0 + dst_register
            ));
        } else {
            const auto imm{ instr.imm32 };
            const auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

            if (src_register != RSP.idx) {
                asmb.inject(char_array(
                    /* LEA */ 0x49, 0x8d, 0x88 + src_register, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
                    /* AND */ 0x48, 0x81, 0xe1, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
                    /* MOV */ 0x4c, 0x89, 0xc0 + 8 * dst_register,
                    /* MULH */ 0x48, 0xf7, 0x24, 0x0e,
                    /* MOV */ 0x49, 0x89, 0xd0 + dst_register
                ));
            } else {
                asmb.inject(char_array(
                    /* LEA */ 0x49, 0x8d, 0x88 + src_register, 0x24, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
                    /* AND */ 0x48, 0x81, 0xe1, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
                    /* MOV */ 0x4c, 0x89, 0xc0 + 8 * dst_register,
                    /* MULH */ 0x48, 0xf7, 0x24, 0x0e,
                    /* MOV */ 0x49, 0x89, 0xd0 + dst_register
                ));
            }
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void ismulhr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Int_Register_Count };
        const uint8_t src_register{ instr.src_register % Int_Register_Count };

        asmb.inject(char_array(
            /* MOV */ 0x4c, 0x89, 0xc0 + 8 * dst_register,
            /* IMULH */ 0x49, 0xf7, 0xe8 + src_register,
            /* MOV */ 0x49, 0x89, 0xd0 + dst_register
        ));

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void ismulhm_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Int_Register_Count };
        const uint8_t src_register{ instr.src_register % Int_Register_Count };

        if (dst_register == src_register) {
            const auto offset{ instr.imm32 & Scratchpad_L3_Mask };
            asmb.inject(char_array(
                /* MOV */ 0x4c, 0x89, 0xc0 + 8 * dst_register,
                /* IMULH */ 0x48, 0xf7, 0xae, b<0>(offset), b<1>(offset), b<2>(offset), b<3>(offset),
                /* MOV */ 0x49, 0x89, 0xd0 + dst_register
            ));
        } else {
            const auto imm{ instr.imm32 };
            const auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

            if (src_register != RSP.idx) {
                asmb.inject(char_array(
                    /* LEA */ 0x49, 0x8d, 0x88 + src_register, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
                    /* AND */ 0x48, 0x81, 0xe1, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
                    /* MOV */ 0x4c, 0x89, 0xc0 + 8 * dst_register,
                    /* IMULH */ 0x48, 0xf7, 0x2c, 0x0e,
                    /* MOV */ 0x49, 0x89, 0xd0 + dst_register
                ));
            } else {
                asmb.inject(char_array(
                    /* LEA */ 0x49, 0x8d, 0x88 + src_register, 0x24, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
                    /* AND */ 0x48, 0x81, 0xe1, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
                    /* MOV */ 0x4c, 0x89, 0xc0 + 8 * dst_register,
                    /* IMULH */ 0x48, 0xf7, 0x2c, 0x0e,
                    /* MOV */ 0x49, 0x89, 0xd0 + dst_register
                ));
            }
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void inegr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Int_Register_Count };
        asmb.inject(char_array(
            /* NEG */ 0x49, 0xf7, 0xd8 + dst_register
        ));
        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void ixorr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Int_Register_Count };
        const uint8_t src_register{ instr.src_register % Int_Register_Count };

        if (dst_register == src_register) {
            const auto imm{ static_cast<int32_t>(instr.imm32) };
            asmb.inject(char_array(
                /* XOR */ 0x49, 0x81, 0xf0 + dst_register, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm)
            ));
        } else {
            asmb.inject(char_array(
                /* XOR */ 0x4d, 0x31, 0xc0 + dst_register + 8 * src_register
            ));
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void ixorm_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Int_Register_Count };
        const uint8_t src_register{ instr.src_register % Int_Register_Count };

        if (dst_register == src_register) {
            const auto offset{ instr.imm32 & Scratchpad_L3_Mask };
            asmb.inject(char_array(
                /* XOR */ 0x4c, 0x33, 0x86 + 8 * dst_register, b<0>(offset), b<1>(offset), b<2>(offset), b<3>(offset)
            ));
        } else {
            const auto imm{ instr.imm32 };
            const auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };
            
            if (src_register != RSP.idx) {
                asmb.inject(char_array(
                    /* LEA */ 0x49, 0x8d, 0x80 + src_register, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
                    /* AND */ 0x48, 0x25, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
                    /* XOR */ 0x4c, 0x33, 0x04 + 8 * dst_register, 0x06
                ));
            } else {
                asmb.inject(char_array(
                    /* LEA */ 0x49, 0x8d, 0x80 + src_register, 0x24, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
                    /* AND */ 0x48, 0x25, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
                    /* XOR */ 0x4c, 0x33, 0x04 + 8 * dst_register, 0x06
                ));
            }
        }

        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void irorr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Int_Register_Count };
        const uint8_t src_register{ instr.src_register % Int_Register_Count };

        if (dst_register == src_register) {
            if (const auto imm = instr.imm32 % 64; imm != 0) {
                asmb.inject(char_array(
                    /* ROR */ 0x49, 0xc1, 0xc8 + dst_register, b<0>(imm)
                ));
            }
        } else {
            asmb.inject(char_array(
                /* MOV */ 0x4c, 0x89, 0xc1 + 8 * src_register,
                /* ROR */ 0x49, 0xd3, 0xc8 + dst_register
            ));
        }

        // Set even for rotate == 0.
        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void irolr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Int_Register_Count };
        const uint8_t src_register{ instr.src_register % Int_Register_Count };

        if (dst_register == src_register) {
            if (const auto imm = instr.imm32 % 64; imm != 0) {
                asmb.inject(char_array(
                    /* ROL */ 0x49, 0xc1, 0xc0 + dst_register, b<0>(imm)
                ));
            }
        } else {
            asmb.inject(char_array(
                /* MOV */ 0x4c, 0x89, 0xc1 + 8 * src_register,
                /* ROL */ 0x49, 0xd3, 0xc0 + dst_register
            ));
        }

        // Set even for rotate == 0.
        setJmpPoint(asmb, reg_usage, dst_register, idx);
    }

    void imulrcp_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Int_Register_Count };

        if (const auto imm = instr.imm32; imm != 0 && !std::has_single_bit(imm)) {
            const auto rcp{ reciprocal(imm) };
            asmb.inject(char_array(
                /* MOV */ 0x48, 0xb8, b<0>(rcp), b<1>(rcp), b<2>(rcp), b<3>(rcp), b<4>(rcp), b<5>(rcp), b<6>(rcp), b<7>(rcp),
                /* IMUL */ 0x4c, 0x0f, 0xaf, 0xc0 + 8 * dst_register
            ));
            setJmpPoint(asmb, reg_usage, dst_register, idx);
        }
    }

    void iswapr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Int_Register_Count };
        const uint8_t src_register{ instr.src_register % Int_Register_Count };

        if (src_register != dst_register) {
            asmb.inject(char_array(
                /* XCHG */ 0x4d, 0x87, 0xc0 + dst_register + 8 * src_register
            ));
            setJmpPoint(asmb, reg_usage, src_register, idx);
            setJmpPoint(asmb, reg_usage, dst_register, idx);
        }
    }

    void cbranch_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Int_Register_Count };
        const uint8_t src_register{ instr.src_register % Int_Register_Count };

        constexpr uint32_t Condition_Mask{ (1 << Rx_Jump_Bits) - 1 };
        const auto shift{ instr.modCond() + Rx_Jump_Offset };
        const auto mem_mask{ Condition_Mask << shift };

        static_assert(Rx_Jump_Offset > 0, "Below simplification requires this assertion");
        int32_t imm{ static_cast<int32_t>(instr.imm32) | (1 << shift) };
        imm &= ~(1ULL << (shift - 1)); // Clear the bit below the condition mask - this limits the number of successive jumps to 2.

        const auto jmp_target{ reg_usage[dst_register] + 1 };
        const auto jmp_offset{ asmb.labelOffset(jmp_target) - 20 };

        asmb.inject(char_array(
            /* ADD */ 0x49, 0x81, 0xc0 + dst_register, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
            /* TEST */ 0x49, 0xf7, 0xc0 + dst_register, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
            /* JZ */  0x0f, 0x84, b<0>(jmp_offset), b<1>(jmp_offset), b<2>(jmp_offset), b<3>(jmp_offset)
        ));

        asmb.label<false>(idx + 1);
        for (auto& reg : reg_usage) reg = idx; // Set all registers as used.
    }

    void fswapr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Int_Register_Count };

        asmb.inject(char_array(
            /* VSHUFPD */ 0xc5, 0xf9 - 8 * dst_register, 0xc6, 0xc0 + 9 * dst_register, 0x01
        ));
    }

    void faddr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Float_Register_Count };
        const uint8_t src_register{ 8 + instr.src_register % Float_Register_Count };

        asmb.inject(char_array(
            /* VADDPD */ 0xc5, 0xf9 - 8 * src_register, 0x58, 0xc0 + 9 * dst_register
        ));
    }

    void faddm_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t src_register{ instr.src_register % Int_Register_Count };
        const uint8_t f_dst_register{ instr.dst_register % Float_Register_Count };
        const auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };
        const auto imm{ instr.imm32 };

        if (src_register != RSP.idx) {
            asmb.inject(char_array(
                /* LEA */ 0x49, 0x8d, 0x80 + src_register, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
                /* AND */ 0x48, 0x25, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
                /* VCVTDQ2PD */ 0xc5, 0x7a, 0xe6, 0x24, 0x06,
                /* VADDPD */ 0xc5, 0x99, 0x58, 0xc0 + 9 * f_dst_register
            ));
        } else {
            asmb.inject(char_array(
                /* LEA */ 0x49, 0x8d, 0x80 + src_register, 0x24, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
                /* AND */ 0x48, 0x25, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
                /* VCVTDQ2PD */ 0xc5, 0x7a, 0xe6, 0x24, 0x06,
                /* VADDPD */ 0xc5, 0x99, 0x58, 0xc0 + 9 * f_dst_register
            ));
        }
    }

    void fsubr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Float_Register_Count };
        const uint8_t src_register{ instr.src_register % Float_Register_Count };

        asmb.inject(char_array(
            /* VSUBPD */ 0xc4, 0xc1, 0x79 - 8 * dst_register, 0x5c, 0xc0 + 8 * dst_register + src_register
        ));
    }

    void fsubm_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t src_register{ instr.src_register % Int_Register_Count };
        const uint8_t f_dst_register{ instr.dst_register % Float_Register_Count };
        const auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };
        const auto imm{ instr.imm32 };

        if (src_register != RSP.idx) {
            asmb.inject(char_array(
                /* LEA */ 0x49, 0x8d, 0x80 + src_register, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
                /* AND */ 0x48, 0x25, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
                /* VCVTDQ2PD */ 0xc5, 0x7a, 0xe6, 0x24, 0x06,
                /* VSUBPD */ 0xc4, 0xc1, 0x79 - 8 * f_dst_register, 0x5c, 0xc4 + 8 * f_dst_register
            ));
        } else {
            asmb.inject(char_array(
                /* LEA */ 0x49, 0x8d, 0x80 + src_register, 0x24, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
                /* AND */ 0x48, 0x25, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
                /* VCVTDQ2PD */ 0xc5, 0x7a, 0xe6, 0x24, 0x06,
                /* VSUBPD */ 0xc4, 0xc1, 0x79 - 8 * f_dst_register, 0x5c, 0xc4 + 8 * f_dst_register
            ));
        }
    }

    void fscalr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Float_Register_Count };

        asmb.inject(char_array(
            /* VPXOR */ 0xc5, 0x89, 0xef, 0xc0 + 9 * dst_register
        ));
    }

    void fmulr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Float_Register_Count };
        const uint8_t src_register{ instr.src_register % Float_Register_Count };

        asmb.inject(char_array(
            /* VMULPD */ 0xc5, 0xb9 - 8 * src_register, 0x59, 0xe4 + 9 * dst_register
        ));
    }

    void fdivm_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t src_register{ instr.src_register % Int_Register_Count };
        const uint8_t f_dst_register{ instr.dst_register % Float_Register_Count };
        const auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };
        const auto imm{ instr.imm32 };

        if (src_register != registers::RSP.idx) {
            asmb.inject(char_array(
                /* VMOVDQU */ 0xc5, 0x7a, 0x6f, 0xaf, 0x18, 0x01, 0x00, 0x00,
                /* LEA */ 0x49, 0x8d, 0x80 + src_register, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
                /* AND */ 0x48, 0x25, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
                /* VCVTDQ2PD */ 0xc5, 0x7a, 0xe6, 0x24, 0x06,
                /* VPAND */ 0xc4, 0x41, 0x19, 0xdb, 0xe7,
                /* VPOR */ 0xc4, 0x41, 0x19, 0xeb, 0xe5,
                /* VDIVPD */ 0xc4, 0xc1, 0x59 - 8 * f_dst_register, 0x5e, 0xe4 + 8 * f_dst_register
            ));
        } else {
            asmb.inject(char_array(
                /* VMOVDQU */ 0xc5, 0x7a, 0x6f, 0xaf, 0x18, 0x01, 0x00, 0x00,
                /* LEA */ 0x49, 0x8d, 0x80 + src_register, 0x24, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
                /* AND */ 0x48, 0x25, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
                /* VCVTDQ2PD */ 0xc5, 0x7a, 0xe6, 0x24, 0x06,
                /* VPAND */ 0xc4, 0x41, 0x19, 0xdb, 0xe7,
                /* VPOR */ 0xc4, 0x41, 0x19, 0xeb, 0xe5,
                /* VDIVPD */ 0xc4, 0xc1, 0x59 - 8 * f_dst_register, 0x5e, 0xe4 + 8 * f_dst_register
            ));
        }
    }

    void fsqrtr_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Float_Register_Count };

        asmb.inject(char_array(
            /* VSQRTPD */ 0xc5, 0xf9, 0x51, 0xe4 + 9 * dst_register
        ));
    }

    void cfround_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t src_register{ instr.src_register % Int_Register_Count };

        if (const auto imm{ instr.imm32 % 64 }; imm != 0) {
            asmb.inject(char_array(
                /* MOV */ 0x4c, 0x89, 0xc0 + 8 * src_register,
                /* ROR */ 0x48, 0xc1, 0xc8, b<0>(imm),
                /* AND */ 0x48, 0x83, 0xe0, 0x03,
                /* ROL */ 0x48, 0xc1, 0xc0, 0x0d,
                /* OR */ 0x48, 0x0d, 0xc0, 0x9f, 0x00, 0x00,
                /* PUSH */ 0x50,
                /* LDMXCSR */ 0x0f, 0xae, 0x14, 0x24,
                /* POP */ 0x58
            ));
        } else {
            asmb.inject(char_array(
                /* MOV */ 0x4c, 0x89, 0xc0 + 8 * src_register,
                /* AND */ 0x48, 0x83, 0xe0, 0x03,
                /* ROL */ 0x48, 0xc1, 0xc0, 0x0d,
                /* OR */ 0x48, 0x0d, 0xc0, 0x9f, 0x00, 0x00,
                /* PUSH */ 0x50,
                /* LDMXCSR */ 0x0f, 0xae, 0x14, 0x24,
                /* POP */ 0x58
            ));
        }
    }

    void istore_cmpl(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const RxInstruction& instr, const uint32_t idx) {
        const uint8_t dst_register{ instr.dst_register % Int_Register_Count };
        const uint8_t src_register{ instr.src_register % Int_Register_Count };
        const auto imm{ static_cast<int32_t>(instr.imm32) };
        constexpr uint32_t L3_Store_Condition{ 14 };

        auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };
        if (instr.modCond() >= L3_Store_Condition) {
            mem_mask = Scratchpad_L3_Mask;
        }

        if (dst_register != registers::RSP.idx) {
            asmb.inject(char_array(
                /* LEA */ 0x49, 0x8d, 0x80 + dst_register, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
                /* AND */ 0x48, 0x25, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
                /* MOV */ 0x4c, 0x89, 0x04 + 8 * src_register, 0x06
            ));
        } else {
            asmb.inject(char_array(
                /* LEA */ 0x49, 0x8d, 0x80 + dst_register, 0x24, b<0>(imm), b<1>(imm), b<2>(imm), b<3>(imm),
                /* AND */ 0x48, 0x25, b<0>(mem_mask), b<1>(mem_mask), b<2>(mem_mask), b<3>(mem_mask),
                /* MOV */ 0x4c, 0x89, 0x04 + 8 * src_register, 0x06
            ));
        }
    }

    namespace {
        // Mark register as used by current instruction and set jump point to next instruction.
        void setJmpPoint(assembler::Context& asmb, std::array<int32_t, Int_Register_Count>& reg_usage, const uint32_t reg_idx, const uint32_t instr_idx) {
            reg_usage[reg_idx] = instr_idx;
            asmb.label<false>(instr_idx + 1);
        };
    }
}

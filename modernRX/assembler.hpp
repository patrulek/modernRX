#pragma once

/*
* This file contains functions for generating binary code for x86_64 architecture.
* It is not a complete assembler, only instructions needed for JIT Compiler used for RandomX programs.
* Code may be a little bit messy and not fully documented as this will be further extended in unknown direction.
*/

#include <array>
#include <iterator>
#include <vector>

#include "alignedallocator.hpp"
#include "assemblerdef.hpp"
#include "cast.hpp"

namespace modernRX::assembler {
    using data_vector = std::vector<uint8_t, AlignedAllocator<uint8_t, 4096>>;

    class Context {
    public:
        [[nodiscard]] explicit Context(const size_t code_size, const size_t data_size = 0) {
            code.reserve(code_size);
            data.reserve(data_size);
            instruction.reserve(32);
        }

        // Broadcasts 4x immediate 64-bit or value from GPR register into registers::RSP[offset] memory.
        // Stack must be aligned to 32 bytes at least.
        template<typename Operand>
        requires (std::is_integral_v<Operand> || std::is_same_v<Operand, Register>)
        constexpr Memory put4qVectorOnStack(const Operand value, const int32_t offset) {
            if constexpr (std::is_integral_v<Operand>) {
                mov(registers::RAX, value);
                vmovq(registers::XMM0, registers::RAX);
            } else {
                vmovq(registers::XMM0, value);
            }
            vpbroadcastq(registers::YMM0, registers::XMM0);
            vmovdqa(registers::RSP[offset], registers::YMM0);
            return registers::RSP[offset];
        }

        // Puts placeholder in the code buffer that will be filled with data pointer before flush.
        // Data pointer cannot be stored immediately because it may change because of reallocation (if data buffer is not big enough).
        // Dst is a register that will be used to mov data pointer.
        // Offset is an offset from the beginning of the data buffer.
        constexpr void movDataPtr(const Register dst, const size_t offset = 0) {
            mov(dst, 0xffffffffffffffff); // put pointer placeholder
            data_ptr_pos.push_back(std::make_pair(code.size() - sizeof(int64_t), offset)); // save code position
        }

        // Stores an immediate value in the data buffer.
        // If any value is returned, it must be consumed by user.
        template<typename Immediate, size_t Bytes, Register reg = registers::DUMMY, typename Ret = std::conditional_t<reg != registers::DUMMY, Memory, void>>
        requires (std::is_integral_v<Immediate>&& Bytes % sizeof(Immediate) == 0)
        [[nodiscard]] constexpr Ret storeImmediate(const Immediate imm) {
            for (uint32_t i = 0; i < Bytes / sizeof(Immediate); ++i) {
                if constexpr (sizeof(Immediate) >= 1) {
                    data.push_back((uint8_t)byte<0>(imm));
                }

                if constexpr (sizeof(Immediate) >= 2) {
                    data.push_back((uint8_t)byte<1>(imm));
                }

                if constexpr (sizeof(Immediate) >= 4) {
                    data.push_back((uint8_t)byte<2>(imm));
                    data.push_back((uint8_t)byte<3>(imm));
                }

                if constexpr (sizeof(Immediate) >= 8) {
                    data.push_back((uint8_t)byte<4>(imm));
                    data.push_back((uint8_t)byte<5>(imm));
                    data.push_back((uint8_t)byte<6>(imm));
                    data.push_back((uint8_t)byte<7>(imm));
                }
            }

            if constexpr (reg.type != RegisterType::DUMMY) {
                return Memory{ static_cast<reg_idx_t>(reg.idx), registers::DUMMY.idx, static_cast<int32_t>(data.size() - Bytes) };
            }
        }

        // Stores an 4 quadwords in the data buffer.
        // If any value is returned, it must be consumed by user.
        template<typename Immediate, Register reg = registers::DUMMY, typename Ret = std::conditional_t<reg != registers::DUMMY, Memory, void>>
        requires (std::is_integral_v<Immediate> && sizeof(Immediate) == 8)
        [[nodiscard]] constexpr Ret storeVector4q(const Immediate imm1, const Immediate imm2, const Immediate imm3, const Immediate imm4) {
            data.push_back((uint8_t)byte<0>(imm1));
            data.push_back((uint8_t)byte<1>(imm1));
            data.push_back((uint8_t)byte<2>(imm1));
            data.push_back((uint8_t)byte<3>(imm1));
            data.push_back((uint8_t)byte<4>(imm1));
            data.push_back((uint8_t)byte<5>(imm1));
            data.push_back((uint8_t)byte<6>(imm1));
            data.push_back((uint8_t)byte<7>(imm1));

            data.push_back((uint8_t)byte<0>(imm2));
            data.push_back((uint8_t)byte<1>(imm2));
            data.push_back((uint8_t)byte<2>(imm2));
            data.push_back((uint8_t)byte<3>(imm2));
            data.push_back((uint8_t)byte<4>(imm2));
            data.push_back((uint8_t)byte<5>(imm2));
            data.push_back((uint8_t)byte<6>(imm2));
            data.push_back((uint8_t)byte<7>(imm2));

            data.push_back((uint8_t)byte<0>(imm3));
            data.push_back((uint8_t)byte<1>(imm3));
            data.push_back((uint8_t)byte<2>(imm3));
            data.push_back((uint8_t)byte<3>(imm3));
            data.push_back((uint8_t)byte<4>(imm3));
            data.push_back((uint8_t)byte<5>(imm3));
            data.push_back((uint8_t)byte<6>(imm3));
            data.push_back((uint8_t)byte<7>(imm3));

            data.push_back((uint8_t)byte<0>(imm4));
            data.push_back((uint8_t)byte<1>(imm4));
            data.push_back((uint8_t)byte<2>(imm4));
            data.push_back((uint8_t)byte<3>(imm4));
            data.push_back((uint8_t)byte<4>(imm4));
            data.push_back((uint8_t)byte<5>(imm4));
            data.push_back((uint8_t)byte<6>(imm4));
            data.push_back((uint8_t)byte<7>(imm4));


            if constexpr (reg.type != RegisterType::DUMMY) {
                return Memory{ static_cast<reg_idx_t>(reg.idx), registers::DUMMY.idx, static_cast<int32_t>(data.size() - 32) };
            }
        }

        constexpr void prefetchnta(const Memory src_reg) {
            encode(rex<uint8_t>(1, 0, 0, src_reg.isHigh()));
            encode(0x0F);
            encode(0x18);
            addr(0, src_reg);
            schedule();
        }

        template<typename Operand>
        constexpr void vpmuludq(const Register dst_reg, const Register src_reg1, const Operand src_reg2) {
            vex256<PP::PP0x66, MM::MM0x0F, Opcode{ 0xf4, -1 }>(dst_reg, src_reg1, src_reg2);
        }

        constexpr void ldmxcsr(const Memory src_reg) {
            encode(0x0f);
            encode(0xae);
            addr(2, src_reg);
            schedule();
        }

        template<typename Operand, typename Control>
        constexpr void vpshufd(const Register dst_reg, const Operand src_reg, const Control control) {
            if (dst_reg.type == RegisterType::YMM) {
                vex256<PP::PP0x66, MM::MM0x0F, Opcode{ 0x70, -1 }>(dst_reg, src_reg, control);
            } else {
                vex128<PP::PP0x66, MM::MM0x0F, Opcode{ 0x70, -1 }>(dst_reg, src_reg, control);
            }
        }

        template<typename Operand, typename Control>
        constexpr void vshufpd(const Register dst_reg, const Register src_reg1, const Operand src_reg2, const Control control) {
            if (dst_reg.type == RegisterType::YMM) {
                vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0xC6, -1 } > (dst_reg, src_reg1, src_reg2, control);
            } else {
                vex128 < PP::PP0x66, MM::MM0x0F, Opcode{ 0xC6, -1 } > (dst_reg, src_reg1, src_reg2, control);
            }
        }

        template<typename Operand>
        constexpr void vcvtdq2pd(const Register dst_reg, const Operand src_reg) {
            if (dst_reg.type == RegisterType::YMM) {
                vex256 < PP::PP0xF3, MM::MM0x0F, Opcode{ 0xE6, -1 } > (dst_reg, src_reg);
            } else {
                vex128 < PP::PP0xF3, MM::MM0x0F, Opcode{ 0xE6, -1 } > (dst_reg, src_reg);
            }
        }

        constexpr void vaddpd(const Register dst_reg, const Register src_reg) {
            if (dst_reg.type == RegisterType::YMM) {
                vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0x58, -1 } > (dst_reg, dst_reg, src_reg);
            } else {
                vex128 < PP::PP0x66, MM::MM0x0F, Opcode{ 0x58, -1 } > (dst_reg, dst_reg, src_reg);
            }
        }

        constexpr void vsubpd(const Register dst_reg, const Register src_reg) {
            if (dst_reg.type == RegisterType::YMM) {
                vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0x5C, -1 } > (dst_reg, dst_reg, src_reg);
            } else {
                vex128 < PP::PP0x66, MM::MM0x0F, Opcode{ 0x5C, -1 } > (dst_reg, dst_reg, src_reg);
            }
        }

        constexpr void vmulpd(const Register dst_reg, const Register src_reg) {
            if (dst_reg.type == RegisterType::YMM) {
                vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0x59, -1 } > (dst_reg, dst_reg, src_reg);
            } else {
                vex128 < PP::PP0x66, MM::MM0x0F, Opcode{ 0x59, -1 } > (dst_reg, dst_reg, src_reg);
            }
        }

        constexpr void vdivpd(const Register dst_reg, const Register src_reg) {
            if (dst_reg.type == RegisterType::YMM) {
                vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0x5E, -1 } > (dst_reg, dst_reg, src_reg);
            } else {
                vex128 < PP::PP0x66, MM::MM0x0F, Opcode{ 0x5E, -1 } > (dst_reg, dst_reg, src_reg);
            }
        }

        constexpr void vsqrtpd(const Register dst_reg) {
            if (dst_reg.type == RegisterType::YMM) {
                vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0x51, -1 } > (dst_reg, dst_reg);
            } else {
                vex128 < PP::PP0x66, MM::MM0x0F, Opcode{ 0x51, -1 } > (dst_reg, dst_reg);
            }
        }

        template<typename Operand>
        constexpr void vpunpcklqdq(const Register dst_reg, const Register src_reg1, const Operand src_reg2) {
            vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0x6c, -1 } > (dst_reg, src_reg1, src_reg2);
        }

        template<typename Operand>
        constexpr void vpunpckhqdq(const Register dst_reg, const Register src_reg1, const Operand src_reg2) {
            vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0x6d, -1 } > (dst_reg, src_reg1, src_reg2);
        }

        template<typename Operand, typename Control>
        constexpr void vperm2i128(const Register dst_reg, const Register src_reg1, const Operand src_reg2, const Control control) {
            //VEX.256.66.0F3A.W0 46 / r ib VPERM2I128 ymm1, ymm2, ymm3 / m256, imm8
            vex256 < PP::PP0x66, MM::MM0x0F3A, Opcode{ 0x46, -1 } > (dst_reg, src_reg1, src_reg2, control);
        }

        constexpr void vzeroupper() {
            encode(vex1<uint8_t>(false));
            encode(vex2<uint8_t>(0, PP::PP0x00, 0, 0));
            encode(0x77);
            schedule();
        }

        // https://stackoverflow.com/a/28827013
        // Multiply packed unsigned quadwords and store high result.
        // This is emulated instruction (not available in AVX2).
        // Requires an 0x00000000ffffffff mask in RSP[64] register.
        // Uses YMM0-YMM3 registers.
        constexpr void vpmulhuq(const Register dst_reg, const Register src_reg1, const Register src_reg2) {
            vpshufd(registers::YMM1, src_reg1, 0xb1); // vpshufd dst
            vpshufd(registers::YMM2, src_reg2, 0xb1); // vpshufd src
            vpmuludq(registers::YMM3, src_reg1, src_reg2); // vpmuludq_w0
            vpmuludq(registers::YMM0, registers::YMM1, registers::YMM2); // vpmuludq_w3
            vpmuludq(registers::YMM2, src_reg1, registers::YMM2); // vpmuludq_w1
            vpsrlq(registers::YMM3, registers::YMM3, 32); // vpsrlq_w0h
            vpaddq(registers::YMM3, registers::YMM3, registers::YMM2); // vpaddq_s1
            vpand(registers::YMM2, registers::YMM3, registers::RSP[64]); // vpand_s1l
            vpmuludq(registers::YMM1, src_reg2, registers::YMM1); // vpmuludq_w2
            vpsrlq(registers::YMM3, registers::YMM3, 32); // vpsrlq_s1h
            vpaddq(registers::YMM0, registers::YMM0, registers::YMM3); // vpaddq_hi
            vpaddq(registers::YMM2, registers::YMM2, registers::YMM1); // vpaddq_s2
            vpsrlq(registers::YMM2, registers::YMM2, 32); // vpsrlq_s2h
            vpaddq(dst_reg, registers::YMM0, registers::YMM2); // vpaddq_ret)
        }



        // https://stackoverflow.com/a/28827013
        // Multiply packed quadwords and store high result.
        // This is emulated instruction (not available in AVX2).
        // Requires YMM5 to be zeroed.
        // Uses YMM0-YMM2 registers.
        constexpr void vpmulhq(const Register dst_reg, const Register src_reg) {
            vpmulhuq(registers::YMM2, dst_reg, src_reg);
            vpcmpgtq(registers::YMM0, registers::YMM5, dst_reg);
            vpand(registers::YMM0, src_reg, registers::YMM0);
            vpsubq(registers::YMM2, registers::YMM2, registers::YMM0);
            vpcmpgtq(registers::YMM1, registers::YMM5, src_reg);
            vpand(registers::YMM1, dst_reg, registers::YMM1);
            vpsubq(dst_reg, registers::YMM2, registers::YMM1);
        }

        // Aligns stack to 64-byte boundary.
        constexpr void alignStack() {
            mov(registers::RAX, registers::RSP);
            mov(registers::RBX, registers::RSP);
            and_(registers::RAX, -64);
            sub(registers::RBX, registers::RAX);
            sub(registers::RSP, registers::RBX);
            push(registers::RBX);
            sub(registers::RSP, 56);
        }


        constexpr void and_(const Register dst, const Register src) {
            encode(rex<uint8_t>(1, dst.isHigh(), 0, src.isHigh()));
            encode(0x23);
            encode(modregrm<uint8_t>(dst.lowIdx(), src.lowIdx()));
            schedule();
        }

        constexpr void and_(const Register dst_reg, const int32_t imm) {
            encode(rex<uint8_t>(1, 0, 0, dst_reg.isHigh()));
            encode(0x81);
            encode(modregrm<uint8_t>(4, dst_reg.lowIdx()));
            encode32(imm);
            schedule();
        }

        constexpr void or_(const Register dst_reg, const int32_t imm) {
            encode(rex<uint8_t>(1, 0, 0, dst_reg.isHigh()));
            encode(0x81);
            encode(modregrm<uint8_t>(1, dst_reg.lowIdx()));
            encode32(imm);
            schedule();
        }

        constexpr void xor_(const Register dst, const Register src) {
            encode(rex<uint8_t>(1, dst.isHigh(), 0, src.isHigh()));
            encode(0x33);
            encode(modregrm<uint8_t>(dst.lowIdx(), src.lowIdx()));
            schedule();
        }

        constexpr void xor_(const Register dst, const int32_t imm) {
            encode(rex<uint8_t>(1, 0, 0, dst.isHigh()));
            encode(0x81);
            encode(modregrm<uint8_t>(6, dst.lowIdx()));
            encode32(imm);
            schedule();
        }

        constexpr void ror(const Register dst, const Register src) {
            mov(registers::RCX, src);
            encode(rex<uint8_t>(1, 0, 0, dst.isHigh()));
            encode(0xd3);
            encode(modregrm<uint8_t>(1, dst.lowIdx()));
            schedule();
        }

        constexpr void ror(const Register dst, const int32_t imm) {
            encode(rex<uint8_t>(1, 0, 0, dst.isHigh()));
            encode(0xc1);
            encode(modregrm<uint8_t>(1, dst.lowIdx()));
            encode((uint8_t)byte<0>(imm));
            schedule();
        }

        constexpr void xchg(const Register dst, const Register src) {
            encode(rex<uint8_t>(1, dst.isHigh(), 0, src.isHigh()));
            encode(0x87);
            encode(modregrm<uint8_t>(dst.lowIdx(), src.lowIdx()));
            schedule();
        }

        constexpr void rol(const Register dst, const Register src) {
            mov(registers::RCX, src);
            encode(rex<uint8_t>(1, 0, 0, dst.isHigh()));
            encode(0xd3);
            encode(modregrm<uint8_t>(0, dst.lowIdx()));
            schedule();
        }

        constexpr void rol(const Register dst, const int32_t imm) {
            encode(rex<uint8_t>(1, 0, 0, dst.isHigh()));
            encode(0xc1);
            encode(modregrm<uint8_t>(0, dst.lowIdx()));
            encode((uint8_t)byte<0>(imm));
            schedule();
        }

        constexpr void test(const Register dst, const int32_t imm) {
            encode(rex<uint8_t>(1, 0, 0, dst.isHigh()));
            encode(0xf7);
            encode(modregrm<uint8_t>(0, dst.lowIdx()));
            encode32(imm);
            schedule();
        }

        constexpr void jz(const int name) {
            const auto rel32{ labels[name] - code.size() - 6 };

            encode(0x0f);
            encode(0x84);
            encode32(static_cast<int32_t>(rel32));
            schedule();
        }

        constexpr void xor_(const Register dst, const Memory src) {
            encode(rex<uint8_t>(1, dst.isHigh(), 0, src.isHigh()));
            encode(0x33);
            addr(dst.lowIdx(), src);
            schedule();
        }

        // Restores stack pointer. alignStack must be called before.
        // Must be called after destruction of all local variables and before popping registers.
        constexpr void unalignStack() {
            add(registers::RSP, 56);
            pop(registers::RAX);
            add(registers::RSP, registers::RAX);
        }

        // https://stackoverflow.com/a/37322570
        // Multiply packed quadwords and store low result.
        // This is emulated instruction (not available in AVX2).
        // Requires an 0xffffffff00000000 mask in YMM4 register.
        // Uses YMM0-YMM2 registers.
        template<typename Operand>
        constexpr void vpmullq(const Register dst_reg, const Register src_reg1, const Operand src_reg2) {
            vpshufd(registers::YMM0, src_reg1, 0xb1);
            // VEX.256.66.0F38.WIG 40 /r VPMULLD ymm1, ymm2, ymm3/m256
            if constexpr (std::is_same_v<Operand, Register>) {
                vex256 < PP::PP0x66, MM::MM0x0F38, Opcode{ 0x40, -1 } > (registers::YMM0, src_reg2, registers::YMM0);
            } else {
                vex256 < PP::PP0x66, MM::MM0x0F38, Opcode{ 0x40, -1 } > (registers::YMM0, registers::YMM0, src_reg2);
            }
            vpsllq(registers::YMM1, registers::YMM0, 32);
            // VEX.256.66.0F.WIG FE /r VPADDD ymm1, ymm2, ymm3/m256
            vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0xfe, -1 } > (registers::YMM1, registers::YMM1, registers::YMM0);
            vpmuludq(registers::YMM2, src_reg1, src_reg2);
            vpand(registers::YMM1, registers::YMM1, registers::YMM4);
            vpaddq(dst_reg, registers::YMM2, registers::YMM1);
        }

        // Broadcasts 64-bit value from XMM register into YMM register.
        // In a case when src is a GPR register, it is first moved to XMM register.
        // In a case when src is an immediate value, it is first moved to RAX register.
        template<typename Operand>
        constexpr void vpbroadcastq(const Register ymm, const Operand src) {
            if constexpr (std::is_same_v<Operand, Register>) {
                if (src.type == RegisterType::GPR) {
                    vmovq(Register::XMM(ymm.idx), src);
                    if (ymm.type == RegisterType::XMM) {
                        vex128 < PP::PP0x66, MM::MM0x0F38, Opcode{ 0x59, -1 } > (ymm, ymm);
                    } else {
                        vex256 < PP::PP0x66, MM::MM0x0F38, Opcode{ 0x59, -1 } > (ymm, Register::XMM(ymm.idx));
                    }
                } else {
                    if (ymm.type == RegisterType::XMM) {
                        vex128 < PP::PP0x66, MM::MM0x0F38, Opcode{ 0x59, -1 } > (ymm, src);
                    } else {
                        vex256 < PP::PP0x66, MM::MM0x0F38, Opcode{ 0x59, -1 } > (ymm, src);
                    }
                }
            } else {
                mov(registers::RAX, src);
                vmovq(ymm, registers::RAX);

                if (ymm.type == RegisterType::XMM) {
                    vex128 < PP::PP0x66, MM::MM0x0F38, Opcode{ 0x59, -1 } > (ymm, ymm);
                } else {
                    vex256 < PP::PP0x66, MM::MM0x0F38, Opcode{ 0x59, -1 } > (ymm, ymm);
                }
            }        
        }

        // Moves 64-bit value from GPR to XMM register.
        constexpr void vmovq(const Register dst, const Register src) {
            // vmovq xmm, gpr: {vex.3B: 0xc4} {vex.0bR0B'00001} {vex.0b1'....'0'66} {opcode: #L:0x6e|#S:0x7e /r} {modrm: 0b11'xmm'gpr}
            if (dst.type == RegisterType::XMM) {
                encode(vex1<uint8_t>(true));
                encode(vex2<uint8_t>(MM::MM0x0F, dst.isLow(), 0, src.isLow()));
                encode(vex3<uint8_t>(0, PP::PP0x66, 1, 0));
                encode(0x6e);
                encode(modregrm<uint8_t>(dst.lowIdx(), src.lowIdx()));
            } else {
                encode(vex1<uint8_t>(true));
                encode(vex2<uint8_t>(MM::MM0x0F, src.isLow(), 0, dst.isLow()));
                encode(vex3<uint8_t>(0, PP::PP0x66, 1, 0));
                encode(0x7e);
                encode(modregrm<uint8_t>(src.lowIdx(), dst.lowIdx()));
            }
            schedule();
        }

        // Extracts 128-bit value from YMM register to XMM register.
        template<typename Control>
        constexpr void vextracti128(const Register dst, const Register src, const Control control) {
            // VEX.256.66.0F3A.W0 19 / r ib VEXTRACTF128 xmm1 / m128, ymm2, imm8
            if (dst.type == RegisterType::XMM) {
                vex256 < PP::PP0x66, MM::MM0x0F3A, Opcode{ 0x39, -1 } > (src, dst, control);
            } else {
                vex256 < PP::PP0x66, MM::MM0x0F3A, Opcode{ 0x39, -1 } > (dst, src, control);
            }
        }

        // Extracts quadword from xmm to gpr register.
        template<typename Control>
        constexpr void vpextrq(const Register dst, const Register src, const Control control) {
            // VEX.128.66.0F3A.W1 16 /r ib VPEXTRQ r64/m64, xmm2, imm8
            encode(vex1<uint8_t>(true));
            encode(vex2<uint8_t>(MM::MM0x0F3A, src.isLow(), 0, dst.isLow()));
            encode(vex3<uint8_t>(0, PP::PP0x66, 1, 0));
            encode(0x16);
            encode(modregrm<uint8_t>(src.lowIdx(), dst.lowIdx()));
            encode((uint8_t)byte<0>(control));
            schedule();
        }

        // Moves 64-bit value from Memory to gpr register.
        constexpr void mov(const Register gpr, const Memory m64, const uint32_t scale = 1, const bool x64 = true) {
            if (x64) {
                encode(rex<uint8_t>(1, gpr.isHigh(), 0, m64.isHigh()));
            }
            encode(0x8b);
            addr(gpr.lowIdx(), m64, scale);
            schedule();
        }

        // Moves 64-bit value from Register to memory.
        constexpr void mov(const Memory m64, const Register src, const uint32_t scale = 1, const bool x64 = true) {
            if (x64) {
                encode(rex<uint8_t>(1, src.isHigh(), m64.index_reg >= 8 && m64.index_reg != registers::DUMMY.idx, m64.isHigh()));
            }
            encode(0x89);
            addr(src.lowIdx(), m64, scale);
            schedule();
        }

        // Moves 64-bit value from Memory to gpr register.
        constexpr void mov(const Register gpr, const Register src) {
            encode(rex<uint8_t>(1, gpr.isHigh(), 0, src.isHigh()));
            encode(0x8b);
            encode(modregrm<uint8_t>(gpr.lowIdx(), src.lowIdx()));
            schedule();
        }

        // Moves 64-bit immediate value into register.
        constexpr void mov64(const Register gpr, const uint64_t imm64) {
            encode(rex<uint8_t>(1, 0, 0, gpr.isHigh()));
            encode(0xb8 + gpr.lowIdx());
            encode64(imm64);
            schedule();
        }

        // Moves 32 or 64-bit immediate value into register.
        constexpr void mov(const Register gpr, const uint64_t imm64) {
            // mov gpr, imm32/64: [rex.w] {opcode: 0xb8+ rd io} {imm64}
            if (imm64 > std::numeric_limits<uint32_t>::max()) {
                encode(rex<uint8_t>(1, 0, 0, gpr.isHigh()));
            } else {
                encode(rex<uint8_t>(0, 0, 0, gpr.isHigh()));
            }

            encode(0xb8 + gpr.lowIdx());
            encode32(static_cast<int32_t>(imm64));

            if (imm64 > std::numeric_limits<uint32_t>::max()) {
                encode((uint8_t)byte<4>(imm64));
                encode((uint8_t)byte<5>(imm64));
                encode((uint8_t)byte<6>(imm64));
                encode((uint8_t)byte<7>(imm64));
            }
            schedule();
        }

        constexpr void shr(const Register gpr, const int32_t imm) {
            encode(rex<uint8_t>(1, 0, 0, gpr.isHigh()));
            encode(0xc1);
            encode(modregrm<uint8_t>(5, gpr.lowIdx()));
            encode((uint8_t)byte<0>(imm));
            schedule();
        }

        // Can jump only backwards.
        constexpr void jne(const int name) {
            const auto rel32{ labels[name] - code.size() - 6 };

            encode(0x0f);
            encode(0x85);
            encode32(static_cast<int32_t>(rel32));
            schedule();
        }


        constexpr void vmovntdq(const Memory dst, const Register src) {
            // VEX.256.66.0F.WIG E7 /r VMOVNTDQ m256, ymm1
            vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0xe7, -1 } > (src, dst);
        }

        // Align to 32 bytes and store label.
        constexpr void label(const int name, const bool align = true) {
            if (align && code.size() % 32 != 0) {
                nop(32 - (code.size() % 32));
            }
            labels[name] = code.size();
        }


        // Both Dst and Src cannot be of Memory type.
        template<typename Dst, typename Src>
        requires (
            (std::is_same_v<Dst, Register> || std::is_same_v<Dst, Memory>) && 
            (std::is_same_v<Src, Register> || std::is_same_v<Src, Memory>) &&
            (!std::is_same_v<Dst, Src> || (std::is_same_v<Dst, Register> && std::is_same_v<Src, Register>))
        )
        constexpr void vmovdqa(const Dst dst, const Src src) {
            // vmovqdu ymmX, ymmword ptr [gpr + offset]: {vex.2B: 0xc5} {vex.0bR'0000'1'66} {opcode: #L:0x6f|#S:0x7f /r} {modrm: 0b00'ymm'gpr} [sib: rsp] [disp8/32]
            if constexpr (std::is_same_v<Dst, Register>) {
                if (dst.type == RegisterType::YMM) {
                    vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0x6f, -1 } > (dst, src);
                } else {
                    vex128<PP::PP0x66, MM::MM0x0F, Opcode{ 0x6f, -1 } > (dst, src);
                }
            } else {
                if (src.type == RegisterType::YMM) {
                    vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0x7f, -1 } > (src, dst);
                } else {
                    vex128<PP::PP0x66, MM::MM0x0F, Opcode{ 0x7f, -1 } > (src, dst);
                }
            }
        }

        // Both Dst and Src cannot be of Memory type.
        template<typename Dst, typename Src>
        requires (
            (std::is_same_v<Dst, Register> || std::is_same_v<Dst, Memory>) &&
            (std::is_same_v<Src, Register> || std::is_same_v<Src, Memory>) &&
            (!std::is_same_v<Dst, Src> || (std::is_same_v<Dst, Register> && std::is_same_v<Src, Register>))
        )
        constexpr void vmovdqu(const Dst dst, const Src src) {
            // vmovqdu ymmX, ymmword ptr [gpr + offset]: {vex.2B: 0xc5} {vex.0bR'0000'1'66} {opcode: #L:0x6f|#S:0x7f /r} {modrm: 0b00'ymm'gpr} [sib: rsp] [disp8/32]
            if constexpr (std::is_same_v<Dst, Register>) {
                if (dst.type == RegisterType::YMM) {
                    vex256 < PP::PP0xF3, MM::MM0x0F, Opcode{ 0x6f, -1 } > (dst, src);
                } else {
                    vex128 < PP::PP0xF3, MM::MM0x0F, Opcode{ 0x6f, -1 } > (dst, src);
                }
            } else {
                if (src.type == RegisterType::YMM) {
                    vex256 < PP::PP0xF3, MM::MM0x0F, Opcode{ 0x7f, -1 } > (src, dst);
                } else {
                    vex128 < PP::PP0xF3, MM::MM0x0F, Opcode{ 0x7f, -1 } > (src, dst);
                }
            }
        }

        template<typename Operand>
        constexpr void vpsrlq(const Register dst_reg, const Register src_reg, const Operand shift) {
            vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0x73, 2 } > (dst_reg, src_reg, shift);
        }

        template<typename Operand>
        constexpr void vpsllq(const Register dst_reg, const Register src_reg, const Operand shift) {
            vex256< PP::PP0x66, MM::MM0x0F, Opcode{ 0x73, 6 }>(dst_reg, src_reg, shift);
        }


        template<typename Operand>
        constexpr void vpsubq(const Register dst_reg, const Register src_reg1, const Operand src_reg2) {
            vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0xfb, -1 } > (dst_reg, src_reg1, src_reg2);
        }

        template<typename Operand>
        constexpr void vpcmpgtq(const Register dst_reg, const Register src_reg1, const Operand src_reg2) {
            vex256< PP::PP0x66, MM::MM0x0F38, Opcode{ 0x37, -1 }>(dst_reg, src_reg1, src_reg2);
        }

        template<typename Operand>
        constexpr void vpaddq(const Register dst_reg, const Register src_reg1, const Operand src_reg2) {
            vex256< PP::PP0x66, MM::MM0x0F, Opcode{ 0xd4, -1 }>(dst_reg, src_reg1, src_reg2);
        }

        template<typename Operand>
        constexpr void vpxor(const Register dst_reg, const Register src_reg1, const Operand src_reg2) {
            if (dst_reg.type == RegisterType::XMM) {
                vex128 < PP::PP0x66, MM::MM0x0F, Opcode{ 0xef, -1 } > (dst_reg, src_reg1, src_reg2);
            } else {
                vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0xef, -1 } > (dst_reg, src_reg1, src_reg2);
            }
        }

        // Convenient function for zeroing YMM register.
        constexpr void vzeroreg(const Register dst_reg) {
            vpxor(dst_reg, dst_reg, dst_reg);
        }

        // Convenient function for broadcasting 4 one's (quadwords equal 1) to YMM register.
        constexpr void vonereg(const Register dst_reg) {
            // VEX.256.66.0F38.WIG 29 / r VPCMPEQQ ymm1, ymm2, ymm3 / m256
            vex256< PP::PP0x66, MM::MM0x0F38, Opcode{ 0x29, -1 }>(dst_reg, dst_reg, dst_reg);
            vpsrlq(dst_reg, dst_reg, 63);
        }

        template<typename Operand>
        constexpr void vpand(const Register dst_reg, const Register src_reg1, const Operand src_reg2) {
            if (dst_reg.type == RegisterType::YMM) {
                vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0xdb, -1 } > (dst_reg, src_reg1, src_reg2);
            } else {
                vex128 < PP::PP0x66, MM::MM0x0F, Opcode{ 0xdb, -1 } > (dst_reg, src_reg1, src_reg2);
            }
        }

        template<typename Operand>
        constexpr void vpor(const Register dst_reg, const Register src_reg1, const Operand src_reg2) {
            if (dst_reg.type == RegisterType::YMM) {
                vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0xeb, -1 } > (dst_reg, src_reg1, src_reg2);
            } else {
                vex128 < PP::PP0x66, MM::MM0x0F, Opcode{ 0xeb, -1 } > (dst_reg, src_reg1, src_reg2);
            }
        }

        // Pushes single GPR register on stack.
        constexpr void push(const Register reg) {
            if (reg.isHigh()) {
                encode(rex<uint8_t>(0, 0, 0, 1));
            }
            encode(0x50 + reg.lowIdx());
            schedule();
        }

        // Pops single GPR register from stack.
        constexpr void pop(const Register reg) {
            if (reg.isHigh()) {
                encode(rex<uint8_t>(0, 0, 0, 1));
            }
            encode(0x58 + reg.lowIdx());
            schedule();
        }

        constexpr void sub(const Register dst, const Register src) {
            encode(rex<uint8_t>(1, dst.isHigh(), 0, src.isHigh()));
            encode(0x2b);
            encode(modregrm<uint8_t>(dst.lowIdx(), src.lowIdx()));
            schedule();
        }

        constexpr void add(const Register dst, const Register src) {
            encode(rex<uint8_t>(1, dst.isHigh(), 0, src.isHigh()));
            encode(0x03);
            encode(modregrm<uint8_t>(dst.lowIdx(), src.lowIdx()));
            schedule();
        }

        constexpr void sub(const Register dst, const int32_t size) {
            if (size >= std::numeric_limits<int8_t>::min() && size <= std::numeric_limits<int8_t>::max()) {
                rexw < Opcode{ 0x83, 5 } > (dst, static_cast<int8_t>(size));
            } else {
                rexw < Opcode{ 0x81, 5 } > (dst, size);
            }
        }

        constexpr void sub(const Register dst, const Memory mem) {
            encode(rex<uint8_t>(1, dst.isHigh(), 0, mem.isHigh()));
            encode(0x2b);
            addr(dst.lowIdx(), mem);
            schedule();
        }

        constexpr void add(const Register dst, const Memory mem) {
            encode(rex<uint8_t>(1, dst.isHigh(), 0, mem.isHigh()));
            encode(0x03);
            addr(dst.lowIdx(), mem);
            schedule();
        }

        constexpr void imul(const Register dst, const Register src) {
            encode(rex<uint8_t>(1, dst.isHigh(), 0, src.isHigh()));
            encode(0x0f);
            encode(0xaf);
            encode(modregrm<uint8_t>(dst.lowIdx(), src.lowIdx()));
            schedule();
        }

        constexpr void imul(const Register dst, const Memory src) {
            encode(rex<uint8_t>(1, dst.isHigh(), 0, src.isHigh()));
            encode(0x0f);
            encode(0xaf);
            addr(dst.lowIdx(), src);
            schedule();
        }

        constexpr void mulh(const Register dst, const Register src) {
            mov(registers::RAX, dst);
            encode(rex<uint8_t>(1, 0, 0, src.isHigh()));
            encode(0xf7);
            encode(modregrm<uint8_t>(4, src.lowIdx()));
            mov(dst, registers::RDX);
            schedule();
        }

        constexpr void mulh(const Register dst, const Memory src) {
            mov(registers::RAX, dst);
            encode(rex<uint8_t>(1, 0, 0, src.isHigh()));
            encode(0xf7);
            addr(4, src);
            mov(dst, registers::RDX);
            schedule();
        }

        constexpr void imulh(const Register dst, const Register src) {
            mov(registers::RAX, dst);
            encode(rex<uint8_t>(1, 0, 0, src.isHigh()));
            encode(0xf7);
            encode(modregrm<uint8_t>(5, src.lowIdx()));
            mov(dst, registers::RDX);
            schedule();
        }

        constexpr void imulh(const Register dst, const Memory src) {
            mov(registers::RAX, dst);
            encode(rex<uint8_t>(1, 0, 0, src.isHigh()));
            encode(0xf7);
            addr(5, src);
            mov(dst, registers::RDX);
            schedule();
        }

        constexpr void imul(const Register dst, const int32_t imm) {
            encode(rex<uint8_t>(1, dst.isHigh(), 0, dst.isHigh()));
            encode(0x69);
            encode(modregrm<uint8_t>(dst.lowIdx(), dst.lowIdx()));
            encode32(imm);
            schedule();
        }

        constexpr void add(const Register dst, const int32_t size) {
            if (size >= std::numeric_limits<int8_t>::min() && size <= std::numeric_limits<int8_t>::max()) {
                rexw < Opcode{ 0x83, 0 } > (dst, static_cast<int8_t>(size));
            } else {
                rexw < Opcode{ 0x81, 0 } > (dst, size);
            }
        }

        constexpr void neg(const Register dst) {
            encode(rex<uint8_t>(1, 0, 0, dst.isHigh()));
            encode(0xf7);
            encode(modregrm<uint8_t>(3, dst.lowIdx()));
            schedule();
        }

        constexpr void lea(const Register dst, const Memory mem, const uint32_t scale = 1) {
            const auto is_sib{ mem.index_reg != registers::DUMMY.idx };
            const auto index_high{ is_sib ? mem.index_reg >= 8 : 0 };

            encode(rex<uint8_t>(1, dst.isHigh(), index_high, mem.isHigh()));
            encode(0x8d);

            // is it sib?
            const auto srcreg{ is_sib ? registers::RSP.idx : mem.lowIdx() };

            if (mem.offset == 0) {
                encode(modregrm<uint8_t>(dst.lowIdx(), srcreg, MOD::MOD00)); // Indirect or SIB mode if RSP used.
            } else if (mem.offset >= std::numeric_limits<int8_t>::min() && mem.offset <= std::numeric_limits<int8_t>::max()) {
                encode(modregrm<uint8_t>(dst.lowIdx(), srcreg, MOD::MOD01)); // Indirect + disp8 or SIB mode + disp8 if RSP used.
            } else {
                encode(modregrm<uint8_t>(dst.lowIdx(), srcreg, MOD::MOD10)); // Indirect + disp32 or SIB mode + disp32 if RSP used.
            }

            // SIB byte.
            if (is_sib || srcreg == registers::RSP.idx) {
                const auto indexreg{ is_sib ? mem.index_reg % 8 : registers::RSP.idx };

                switch (scale) {
                case 1:
                    encode(sib<uint8_t>(SCALE::SS1, Register::GPR(indexreg), Register::GPR(mem.lowIdx())));
                    break;
                case 2:
                    encode(sib<uint8_t>(SCALE::SS2, Register::GPR(indexreg), Register::GPR(mem.lowIdx())));
                    break;
                case 4:
                    encode(sib<uint8_t>(SCALE::SS4, Register::GPR(indexreg), Register::GPR(mem.lowIdx())));
                    break;
                case 8:
                    encode(sib<uint8_t>(SCALE::SS8, Register::GPR(indexreg), Register::GPR(mem.lowIdx())));
                    break;
                default:
                    std::unreachable();
                }
            }

            // Disp8
            if (mem.offset != 0) {
                encode((uint8_t)byte<0>(mem.offset));
            }

            // Disp32
            if (mem.offset < std::numeric_limits<int8_t>::min() || mem.offset > std::numeric_limits<int8_t>::max()) {
                encode((uint8_t)byte<1>(mem.offset));
                encode((uint8_t)byte<2>(mem.offset));
                encode((uint8_t)byte<3>(mem.offset));
            }
            schedule();
        }

        // Generates binary code for pushing registers on stack.
        // For general purpose registers it uses PUSH instruction.
        // For XMM registers it expands stack and copies register to stack.
        // Example:
        // context.push(RAX, R8, XMM1, XMM0) will generate:
        //
        // push rax
        // push r8
        // sub rsp, 0x20
        // vmovqdu xmmword ptr [rsp], xmm1
        // vmovqdu xmmword ptr [rsp + 0x10], xmm0
        template<typename... Reg>
        requires (std::is_same_v<Reg, Register> && ...)
        constexpr void push(Reg... regs) {
            static_assert(sizeof...(regs) > 0);

            std::array<Register, sizeof...(regs)> xmm_regs{};
            int32_t xmm_regs_cnt{ 0 };

            for (const auto& reg : { regs... }) {
                // General purpose registers are pushed on stack using PUSH instruction: 
                if (reg.type == RegisterType::GPR) {
                    push(reg);
                }
                // XMM registers are pushed on stack by expanding stack and copying register to stack.
                // Lets just count how many XMM registers was passed and push them at the end, because they may be between general purpose registers.
                else if (reg.type == RegisterType::XMM) {
                    xmm_regs[xmm_regs_cnt++] = reg;
                }
            }

            if (xmm_regs_cnt > 0) {
                const int32_t stack_size{ xmm_regs_cnt * Register::XMM(0).size() };
                sub(registers::RSP, stack_size);

                for (int32_t i = xmm_regs_cnt - 1; i >= 0; --i) {
                    const auto& reg{ xmm_regs[i] };
                    const int32_t stack_offset{ (xmm_regs_cnt - 1 - i) * reg.size() };

                    vmovdqu(registers::RSP[stack_offset], reg);
                }
            }
        }

        // Generates binary code for poping registers from stack.
        // For general purpose registers it uses POP instruction.
        // For XMM registers it copies values from stack to registers.
        // It is important to use this function with registers in reverse order of adequate push function.
        // Example:
        // context.pop(XMM0, XMM1, R8, RAX) will generate:
        //
        // vmovqdu xmm0, xmmword ptr [rsp + 0x10]
        // vmovqdu xmm1, xmmword ptr [rsp]
        // add rsp, 0x20
        // pop r8
        // pop rax
        template<typename... Reg>
        requires (std::is_same_v<Reg, Register> && ...)
        constexpr void pop(Reg... regs) {
            static_assert(sizeof...(regs) > 0);

            std::array<Register, sizeof...(regs)> xmm_regs{};
            int32_t xmm_regs_cnt{ 0 };

            for (const auto& reg : { regs... }) {
                // XMM registers are popped from stack by copying values from stack back to register and shrinking stack.
                // Lets just count how many XMM registers was passed and pop them right after, when stack size reserved for them is known.
                if (reg.type == RegisterType::XMM) {
                    xmm_regs[xmm_regs_cnt++] = reg;
                }
            }

            if (xmm_regs_cnt > 0) {
                for (int32_t i = 0; i < xmm_regs_cnt; ++i) {
                    const auto& reg{ xmm_regs[i] };
                    const int32_t stack_offset{ i * reg.size() };

                    vmovdqu(reg, registers::RSP[stack_offset]);
                }

                const int32_t stack_size{ xmm_regs_cnt * Register::XMM(0).size() };
                add(registers::RSP, stack_size);
            }

            for (const auto& reg : { regs... }) {
                // General purpose registers are popped from stack using POP instruction: 
                if (reg.type == RegisterType::GPR) {
                    pop(reg);
                }
            }
        }

        // Generates nop instructions for given size in bytes.
        constexpr void nop(const int32_t size) {
            int s = size;
            while (s > 15) {
                code.append_range(std::vector<uint8_t>{ 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x2e, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00}); 
                s -= 15;
            }
            switch (s % 16) {
            case 1: code.push_back(0x90); --s; break;
            case 2: code.append_range(std::vector<uint8_t>{ 0x66, 0x90 }); s -= 2; break;
            case 3: code.append_range(std::vector<uint8_t>{ 0x0f, 0x1f, 0x00 }); s -= 3; break;
            case 4: code.append_range(std::vector<uint8_t>{ 0x0f, 0x1f, 0x40, 0x00 }); s -= 4; break;
            case 5: code.append_range(std::vector<uint8_t>{ 0x0f, 0x1f, 0x44, 0x00, 0x00 }); s -= 5; break;
            case 6: code.append_range(std::vector<uint8_t>{ 0x66, 0x0f, 0x1f, 0x44, 0x00, 0x00 }); s -= 6; break;
            case 7: code.append_range(std::vector<uint8_t>{ 0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00 }); s -= 7; break;
            case 8: code.append_range(std::vector<uint8_t>{ 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 }); s -= 8; break;
            case 9: code.append_range(std::vector<uint8_t>{ 0x66, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 }); s -= 9; break;
            case 10: code.append_range(std::vector<uint8_t>{ 0x66, 0x2e, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 }); s -= 10; break;
            case 11: code.append_range(std::vector<uint8_t>{ 0x66, 0x66, 0x2e, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 }); s -= 11; break;
            case 12: code.append_range(std::vector<uint8_t>{ 0x66, 0x66, 0x66, 0x2e, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 }); s -= 12; break;
            case 13: code.append_range(std::vector<uint8_t>{ 0x66, 0x66, 0x66, 0x66, 0x2e, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 }); s -= 13; break;
            case 14: code.append_range(std::vector<uint8_t>{ 0x66, 0x66, 0x66, 0x66, 0x66, 0x2e, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 }); s -= 14; break;
            case 15: code.append_range(std::vector<uint8_t>{ 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x2e, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00}); s -= 15; break;
            default: std::unreachable();
            }
        }

        // Generates ret instruction if previous byte isnt ret opcode already.
        constexpr void ret() {
            if (code.back() == 0xc3) {
                return;
            }

            encode(0xc3);
            schedule();
        }

        // Flushes generated code that results in giving away ownership of the buffer.
        [[nodiscard]] constexpr std::vector<uint8_t> flushCode() noexcept {
            return std::move(code);
        }

        // Flushes generated data that results in giving away ownership of the buffer.
        // Fills data pointers in code.
        [[nodiscard]] constexpr data_vector flushData() noexcept {
            for (const auto& [code_pos, data_offset] : data_ptr_pos) {
                const auto dp{ dataPtr() + data_offset };
                std::memcpy(code.data() + code_pos, &dp, sizeof(int64_t));
            }

            return std::move(data);
        }

        [[nodiscard]] constexpr const uint8_t* dataPtr() noexcept {
            return data.data();
        }

        std::vector<std::pair<size_t, size_t>> data_ptr_pos;
        std::vector<uint8_t> code;
        data_vector data;
        std::vector<uint8_t> instruction;
        std::array<size_t, 512> labels{};

        constexpr void schedule() {
            if (instruction.empty()) {
                return;
            }

            constexpr uint32_t align{ 4096 };
            if ((code.size() + instruction.size() != align) && code.size() % align > (code.size() + instruction.size()) % align) {
                nop(align - (code.size() % align));
            }

            code.insert(code.end(), instruction.begin(), instruction.end());
            instruction.clear();
        }

        constexpr void encode(const uint8_t byte) {
            instruction.push_back(byte);
        }

        constexpr void encode32(const int32_t bytes) {
            instruction.append_range(span_cast<const uint8_t, sizeof(int32_t)>(bytes));
        }

        constexpr void encode64(const int64_t bytes) {
            instruction.append_range(span_cast<const uint8_t, sizeof(int64_t)>(bytes));
        }


        constexpr void addr(const reg_idx_t dst, const Memory& src, const uint32_t scale = 1) {
            // RIP relative addressing (displacement only).
            if (src.lowIdx() == registers::RBP.idx && src.index_reg == registers::DUMMY.idx && src.rip) {
                encode(modregrm<uint8_t>(0, registers::RBP.idx, MOD::MOD00)); // Indirect or SIB mode if RSP used.
                encode32(src.offset);
                return;
            }

            // its sib here
            const auto is_sib{ src.index_reg != registers::DUMMY.idx };
            const auto srcreg{ is_sib ? registers::RSP.idx : src.lowIdx() };
            const auto basereg{ src.lowIdx() };

            if (src.offset == 0 && basereg != registers::RBP.idx) {
                encode(modregrm<uint8_t>(dst % 8, srcreg, MOD::MOD00)); // Indirect or SIB mode if RSP used.
            } else if (src.offset >= std::numeric_limits<int8_t>::min() && src.offset <= std::numeric_limits<int8_t>::max()) {
                encode(modregrm<uint8_t>(dst % 8, srcreg, MOD::MOD01)); // Indirect + disp8 or SIB mode + disp8 if RSP used.
            } else {
                encode(modregrm<uint8_t>(dst % 8, srcreg, MOD::MOD10)); // Indirect + disp32 or SIB mode + disp32 if RSP used.
            }

            // SIB byte.
            if (is_sib || srcreg == registers::RSP.idx) {
                const auto indexreg{ is_sib ? src.index_reg % 8 : registers::RSP.idx };

                switch (scale) {
                case 1:
                    encode(sib<uint8_t>(SCALE::SS1, Register::GPR(indexreg), Register::GPR(src.lowIdx())));
                    break;
                case 2:
                    encode(sib<uint8_t>(SCALE::SS2, Register::GPR(indexreg), Register::GPR(src.lowIdx())));
                    break;
                case 4:
                    encode(sib<uint8_t>(SCALE::SS4, Register::GPR(indexreg), Register::GPR(src.lowIdx())));
                    break;
                case 8:
                    encode(sib<uint8_t>(SCALE::SS8, Register::GPR(indexreg), Register::GPR(src.lowIdx())));
                    break;
                default:
                    std::unreachable();
                }
            }

            // Disp8
            if (src.offset != 0 || basereg == registers::RBP.idx) {
                encode((uint8_t)byte<0>(src.offset));
            }

            // Disp32
            if (src.offset < std::numeric_limits<int8_t>::min() || src.offset > std::numeric_limits<int8_t>::max()) {
                encode((uint8_t)byte<1>(src.offset));
                encode((uint8_t)byte<2>(src.offset));
                encode((uint8_t)byte<3>(src.offset));
            }
        }

        // Generates instruction with REX.W prefix and Register/IMM as operands.
        template<Opcode opcode, typename Imm>
        requires (std::is_same_v<Imm, int8_t> || std::is_same_v<Imm, int32_t> || std::is_same_v<Imm, int64_t>)
        constexpr void rexw(const Register dst, const Imm imm) {
            encode(rex<uint8_t>(1, 0, 0, dst.isHigh()));
            encode(opcode.code);
            encode(modregrm<uint8_t>(std::max<reg_idx_t>(opcode.mod, 0), dst.lowIdx()));

            encode((uint8_t)byte<0>(imm));

            if constexpr (std::is_same_v<Imm, int32_t>) {
                encode((uint8_t)byte<1>(imm));
                encode((uint8_t)byte<2>(imm));
                encode((uint8_t)byte<3>(imm));
            }

            if constexpr (std::is_same_v<Imm, int64_t>) {
                encode((uint8_t)byte<4>(imm));
                encode((uint8_t)byte<5>(imm));
                encode((uint8_t)byte<6>(imm));
                encode((uint8_t)byte<7>(imm));
            }

            schedule();
        }

        // Generates instruction with VEX256 prefix and Register/Register as operands (eg. vpbroadcastq).
        template<PP pp, MM mm, Opcode opcode, uint8_t w = 0>
        constexpr void vex256(const Register dst, const Register src1) {
            if constexpr (mm == MM::MM0x0F) {
                if (src1.isLow()) {
                    // INSTR ymmD, ymmS1, ymmS2: {vex.2B: 0xc5} {vex.0b1'ffff'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
                    encode(vex1<uint8_t>(false));
                    encode(vex2<uint8_t>(0, pp, dst.isLow()));
                } else {
                    // INSTR ymmD, ymmS1, ymmS2: {vex.3B: 0xc4} {vex.0bR0B'00001} {vex.0bW'ffff'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
                    encode(vex1<uint8_t>(true));
                    encode(vex2<uint8_t>(MM::MM0x0F, dst.isLow(), 0, src1.isLow()));
                    encode(vex3<uint8_t>(0, pp, w));
                }
            } else {
                encode(vex1<uint8_t>(true));
                encode(vex2<uint8_t>(mm, dst.isLow(), 0, src1.isLow()));
                encode(vex3<uint8_t>(0, pp, w));
            }

            encode(opcode.code);
            encode(modregrm<uint8_t>(dst.lowIdx(), src1.lowIdx()));
            schedule();
        }

        // Generates instruction with VEX128 prefix and Register/Register as operands (eg. vmovdqa).
        template<PP pp, MM mm, Opcode opcode, uint8_t w = 0>
        constexpr void vex128(const Register dst, const Register src1) {
            const reg_idx_t vvvv{ 0 };

            if constexpr (mm == MM::MM0x0F) {
                if (src1.isLow()) {
                    // INSTR ymmD, ymmS1, ymmS2: {vex.2B: 0xc5} {vex.0b1'ffff'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
                    encode(vex1<uint8_t>(false));
                    encode(vex2<uint8_t>(vvvv, pp, dst.isLow(), 0));
                } else {
                    // INSTR ymmD, ymmS1, ymmS2: {vex.3B: 0xc4} {vex.0bR0B'00001} {vex.0bW'ffff'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
                    encode(vex1<uint8_t>(true));
                    encode(vex2<uint8_t>(MM::MM0x0F, dst.isLow(), 0, src1.isLow()));
                    encode(vex3<uint8_t>(vvvv, pp, w, 0));
                }
            } else {
                encode(vex1<uint8_t>(true));
                encode(vex2<uint8_t>(mm, dst.isLow(), 0, src1.isLow()));
                encode(vex3<uint8_t>(vvvv, pp, w, 0));
            }

            encode(opcode.code);
            encode(modregrm<uint8_t>(dst.lowIdx(), src1.lowIdx()));
            schedule();
        }

        template<PP pp, MM mm, Opcode opcode, uint8_t w = 0>
        constexpr void vex128(const Register dst, const Register src1, const int imm32) {
            const reg_idx_t vvvv{ opcode.mod > -1 ? dst.idx : uint8_t(0) };
            const reg_idx_t reg{ opcode.mod > -1 ? (uint8_t)opcode.mod : dst.lowIdx() };

            if constexpr (mm == MM::MM0x0F) {
                if (src1.isLow()) {
                    // INSTR ymmD, ymmS1, imm8: {vex.2B: 0xc5} {vex.0b1'dddd'1'66} {opcode.code /opcode.mod} {modrm: 0b11'mod'ss!}
                    encode(vex1<uint8_t>(false));
                    encode(vex2<uint8_t>(vvvv, pp, dst.isLow(), 0));
                } else {
                    // INSTR ymmD, ymmS1, imm8: {vex.3B: 0xc4} {vex.0b000'00001} {vex.0bW'dddd'1'66} {opcode.code /opcode.mod} {modrm: 0b11'mod'ss!}
                    encode(vex1<uint8_t>(true));
                    encode(vex2<uint8_t>(MM::MM0x0F, dst.isLow(), 0, src1.isLow()));
                    encode(vex3<uint8_t>(vvvv, pp, w, 0));
                }
            } else {
                encode(vex1<uint8_t>(true));
                encode(vex2<uint8_t>(mm, dst.isLow(), 0, src1.isLow()));
                encode(vex3<uint8_t>(vvvv, pp, w, 0));
            }

            encode(opcode.code);
            encode(modregrm<uint8_t>(reg, src1.lowIdx()));
            encode((uint8_t)byte<0>(imm32));
            schedule();
        }

        // Generates instruction with VEX256 prefix and Register/Memory as operands (eg. vmovdqa).
        template<PP pp, MM mm, Opcode opcode, uint8_t w = 0>
        constexpr void vex256(const Register dst, const Memory src1) {
            if constexpr (mm == MM::MM0x0F) {
                if (src1.isLow()) {
                    // INSTR ymmD, ymmS1, ymmS2: {vex.2B: 0xc5} {vex.0b1'ffff'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
                    encode(vex1<uint8_t>(false));
                    encode(vex2<uint8_t>(0, pp, dst.isLow()));
                } else {
                    // INSTR ymmD, ymmS1, ymmS2: {vex.3B: 0xc4} {vex.0bR0B'00001} {vex.0bW'ffff'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
                    encode(vex1<uint8_t>(true));
                    encode(vex2<uint8_t>(MM::MM0x0F, dst.isLow(), 0, src1.isLow()));
                    encode(vex3<uint8_t>(0, pp, w));
                }
            } else {
                encode(vex1<uint8_t>(true));
                encode(vex2<uint8_t>(mm, dst.isLow(), 0, src1.isLow()));
                encode(vex3<uint8_t>(0, pp, w));
            }

            encode(opcode.code);
            addr(dst.idx, src1);
            schedule();
        }

        // Generates instruction with VEX128 prefix and Register/Memory as operands (eg. vmovdqa).
        template<PP pp, MM mm, Opcode opcode, uint8_t w = 0>
        constexpr void vex128(const Register dst, const Memory src1) {
            if constexpr (mm == MM::MM0x0F) {
                if (src1.isLow()) {
                    // INSTR ymmD, ymmS1, ymmS2: {vex.2B: 0xc5} {vex.0b1'ffff'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
                    encode(vex1<uint8_t>(false));
                    encode(vex2<uint8_t>(0, pp, dst.isLow(), 0));
                } else {
                    // INSTR ymmD, ymmS1, ymmS2: {vex.3B: 0xc4} {vex.0bR0B'00001} {vex.0bW'ffff'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
                    encode(vex1<uint8_t>(true));
                    encode(vex2<uint8_t>(MM::MM0x0F, dst.isLow()));
                    encode(vex3<uint8_t>(0, pp, w, 0));
                }
            } else {
                encode(vex1<uint8_t>(true));
                encode(vex2<uint8_t>(mm, dst.isLow(), 0, src1.isLow()));
                encode(vex3<uint8_t>(0, pp, w, 0));
            }

            encode(opcode.code);
            addr(dst.idx, src1);
            schedule();
        }

        // Generates instruction with VEX256 prefix and Register/Register/Register as operands (eg. vpxor).
        template<PP pp, MM mm, Opcode opcode, uint8_t w = 0>
        constexpr void vex256(const Register dst, const Register src1, const Register src2) {
            if constexpr (mm == MM::MM0x0F) {
                if (src2.isLow() && dst.isLow()) {
                    // INSTR ymmD, ymmS1, ymmS2: {vex.2B: 0xc5} {vex.0b1'sss!'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
                    encode(vex1<uint8_t>(false));
                    encode(vex2<uint8_t>(src1.idx, pp, 1));
                } else {
                    // INSTR ymmD, ymmS1, ymmS2: {vex.3B: 0xc4} {vex.0bR0B'00001} {vex.0bW'sss!'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
                    encode(vex1<uint8_t>(true));
                    encode(vex2<uint8_t>(MM::MM0x0F, dst.isLow(), 0, src2.isLow()));
                    encode(vex3<uint8_t>(src1.idx, pp, w));
                }
            } else {
                encode(vex1<uint8_t>(true));
                encode(vex2<uint8_t>(mm, dst.isLow(), 0, src2.isLow()));
                encode(vex3<uint8_t>(src1.idx, pp, w));
            }

            encode(opcode.code);
            encode(modregrm<uint8_t>(dst.lowIdx(), src2.lowIdx()));
            schedule();
        }

        // Generates instruction with VEX256 prefix and Register/Register/Register as operands (eg. vpxor).
        template<PP pp, MM mm, Opcode opcode, uint8_t w = 0>
        constexpr void vex128(const Register dst, const Register src1, const Register src2) {
            if constexpr (mm == MM::MM0x0F) {
                if (src2.isLow() && dst.isLow()) {
                    // INSTR ymmD, ymmS1, ymmS2: {vex.2B: 0xc5} {vex.0b1'sss!'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
                    encode(vex1<uint8_t>(false));
                    encode(vex2<uint8_t>(src1.idx, pp, 1, 0));
                } else {
                    // INSTR ymmD, ymmS1, ymmS2: {vex.3B: 0xc4} {vex.0bR0B'00001} {vex.0bW'sss!'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
                    encode(vex1<uint8_t>(true));
                    encode(vex2<uint8_t>(MM::MM0x0F, dst.isLow(), 0, src2.isLow()));
                    encode(vex3<uint8_t>(src1.idx, pp, w, 0));
                }
            } else {
                encode(vex1<uint8_t>(true));
                encode(vex2<uint8_t>(mm, dst.isLow(), 0, src2.isLow()));
                encode(vex3<uint8_t>(src1.idx, pp, w, 0));
            }

            encode(opcode.code);
            encode(modregrm<uint8_t>(dst.lowIdx(), src2.lowIdx()));
            schedule();
        }

        // Generates instruction with VEX256 prefix and Register/Register/Register as operands (eg. vpxor).
        template<PP pp, MM mm, Opcode opcode, uint8_t w = 0>
        constexpr void vex256(const Register dst, const Register src1, const Memory src2) {
            if constexpr (mm == MM::MM0x0F) {
                if (src2.isLow()) {
                    // INSTR ymmD, ymmS1, ymmS2: {vex.2B: 0xc5} {vex.0b1'sss!'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
                    encode(vex1<uint8_t>(false));
                    encode(vex2<uint8_t>(src1.idx, pp, dst.isLow()));
                } else {
                    // INSTR ymmD, ymmS1, ymmS2: {vex.3B: 0xc4} {vex.0bR0B'00001} {vex.0bW'sss!'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
                    encode(vex1<uint8_t>(true));
                    encode(vex2<uint8_t>(MM::MM0x0F, dst.isLow(), 0, src2.isLow()));
                    encode(vex3<uint8_t>(src1.idx, pp, w));
                }
            } else {
                encode(vex1<uint8_t>(true));
                encode(vex2<uint8_t>(mm, dst.isLow(), 0, src2.isLow()));
                encode(vex3<uint8_t>(src1.idx, pp, w));
            }

            encode(opcode.code);
            addr(dst.idx, src2);
            schedule();
        }

        // Generates instruction with VEX256 prefix and Register/Register/Register as operands (eg. vpxor).
        template<PP pp, MM mm, Opcode opcode, uint8_t w = 0>
        constexpr void vex128(const Register dst, const Register src1, const Memory src2) {
            if constexpr (mm == MM::MM0x0F) {
                if (src2.isLow()) {
                    // INSTR ymmD, ymmS1, ymmS2: {vex.2B: 0xc5} {vex.0b1'sss!'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
                    encode(vex1<uint8_t>(false));
                    encode(vex2<uint8_t>(src1.idx, pp, dst.isLow(), 0));
                } else {
                    // INSTR ymmD, ymmS1, ymmS2: {vex.3B: 0xc4} {vex.0bR0B'00001} {vex.0bW'sss!'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
                    encode(vex1<uint8_t>(true));
                    encode(vex2<uint8_t>(MM::MM0x0F, dst.isLow(), 0, src2.isLow()));
                    encode(vex3<uint8_t>(src1.idx, pp, w, 0));
                }
            } else {
                encode(vex1<uint8_t>(true));
                encode(vex2<uint8_t>(mm, dst.isLow(), 0, src2.isLow()));
                encode(vex3<uint8_t>(src1.idx, pp, w, 0));
            }

            encode(opcode.code);
            addr(dst.idx, src2);
            schedule();
        }

        // Generates instruction with VEX256 prefix and Register/Register/Imm as operands (eg. vpsllq).
        template<PP pp, MM mm, Opcode opcode, uint8_t w = 0>
        constexpr void vex256(const Register dst, const Register src1, const int imm32) {
            const reg_idx_t vvvv{ opcode.mod > -1 ? dst.idx : uint8_t(0) };
            const reg_idx_t reg{ opcode.mod > -1 ? (uint8_t)opcode.mod : dst.lowIdx() };

            if constexpr (mm == MM::MM0x0F) {
                if (src1.isLow()) {
                    // INSTR ymmD, ymmS1, imm8: {vex.2B: 0xc5} {vex.0b1'dddd'1'66} {opcode.code /opcode.mod} {modrm: 0b11'mod'ss!}
                    encode(vex1<uint8_t>(false));
                    encode(vex2<uint8_t>(vvvv, pp, 1));
                } else {
                    // INSTR ymmD, ymmS1, imm8: {vex.3B: 0xc4} {vex.0b000'00001} {vex.0bW'dddd'1'66} {opcode.code /opcode.mod} {modrm: 0b11'mod'ss!}
                    encode(vex1<uint8_t>(true));
                    encode(vex2<uint8_t>(MM::MM0x0F, dst.isLow(), 0, src1.isLow()));
                    encode(vex3<uint8_t>(vvvv, pp, w));
                }
            } else {
                encode(vex1<uint8_t>(true));
                encode(vex2<uint8_t>(mm, dst.isLow(), 0, src1.isLow()));
                encode(vex3<uint8_t>(vvvv, pp, w));
            }

            encode(opcode.code);
            encode(modregrm<uint8_t>(reg, src1.lowIdx()));
            encode((uint8_t)byte<0>(imm32));
            schedule();
        }

        // Generates instruction with VEX256 prefix and Register/Register/Register/Imm as operands (eg. vperm2i128).
        template<PP pp, MM mm, Opcode opcode, uint8_t w = 0>
        constexpr void vex256(const Register dst, const Register src1, const Register src2, const int imm32) {
            const reg_idx_t vvvv{ src1.idx };
            const reg_idx_t reg{ opcode.mod > -1 ? (uint8_t)opcode.mod : dst.lowIdx() };
            const reg_idx_t rm{ src2.lowIdx() };

            if constexpr (mm == MM::MM0x0F) {
                if (src1.isLow()) {
                    // INSTR ymmD, ymmS1, imm8: {vex.2B: 0xc5} {vex.0b1'dddd'1'66} {opcode.code /opcode.mod} {modrm: 0b11'mod'ss!}
                    encode(vex1<uint8_t>(false));
                    encode(vex2<uint8_t>(vvvv, pp, 1));
                } else {
                    // INSTR ymmD, ymmS1, imm8: {vex.3B: 0xc4} {vex.0b000'00001} {vex.0bW'dddd'1'66} {opcode.code /opcode.mod} {modrm: 0b11'mod'ss!}
                    encode(vex1<uint8_t>(true));
                    encode(vex2<uint8_t>(MM::MM0x0F, dst.isLow(), 0, src2.isLow()));
                    encode(vex3<uint8_t>(vvvv, pp, w));
                }
            } else {
                encode(vex1<uint8_t>(true));
                encode(vex2<uint8_t>(mm, dst.isLow(), 0, src2.isLow()));
                encode(vex3<uint8_t>(vvvv, pp, w));
            }

            encode(opcode.code);
            encode(modregrm<uint8_t>(reg, rm));
            encode((uint8_t)byte<0>(imm32));
            schedule();
        }

        // Generates instruction with VEX256 prefix and Register/Register/Register/Imm as operands (eg. vperm2i128).
        template<PP pp, MM mm, Opcode opcode, uint8_t w = 0>
        constexpr void vex128(const Register dst, const Register src1, const Register src2, const int imm32) {
            const reg_idx_t vvvv{ src1.idx };
            const reg_idx_t reg{ opcode.mod > -1 ? (uint8_t)opcode.mod : dst.lowIdx() };
            const reg_idx_t rm{ src2.lowIdx() };

            if constexpr (mm == MM::MM0x0F) {
                if (src1.isLow()) {
                    // INSTR ymmD, ymmS1, imm8: {vex.2B: 0xc5} {vex.0b1'dddd'1'66} {opcode.code /opcode.mod} {modrm: 0b11'mod'ss!}
                    encode(vex1<uint8_t>(false));
                    encode(vex2<uint8_t>(vvvv, pp, 1, 0));
                } else {
                    // INSTR ymmD, ymmS1, imm8: {vex.3B: 0xc4} {vex.0b000'00001} {vex.0bW'dddd'1'66} {opcode.code /opcode.mod} {modrm: 0b11'mod'ss!}
                    encode(vex1<uint8_t>(true));
                    encode(vex2<uint8_t>(MM::MM0x0F, dst.isLow(), 0, src2.isLow()));
                    encode(vex3<uint8_t>(vvvv, pp, w, 0));
                }
            } else {
                encode(vex1<uint8_t>(true));
                encode(vex2<uint8_t>(mm, dst.isLow(), 0, src2.isLow()));
                encode(vex3<uint8_t>(vvvv, pp, w, 0));
            }

            encode(opcode.code);
            encode(modregrm<uint8_t>(reg, rm));
            encode((uint8_t)byte<0>(imm32));
            schedule();
        }
    };
}

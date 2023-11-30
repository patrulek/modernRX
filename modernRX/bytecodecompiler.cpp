#include "bytecodecompiler.hpp"
#include "randomxparams.hpp"
#include "reciprocal.hpp"
#include "sse.hpp"

namespace modernRX {
    namespace {
        constexpr uint32_t Scratchpad_L1_Mask{ (Rx_Scratchpad_L1_Size - 1) & ~7 }; // L1 cache 8-byte alignment mask.
        constexpr uint32_t Scratchpad_L2_Mask{ (Rx_Scratchpad_L2_Size - 1) & ~7 }; // L2 cache 8-byte alignment mask.
        constexpr uint32_t Scratchpad_L3_Mask{ (Rx_Scratchpad_L3_Size - 1) & ~7 }; // L3 cache 8-byte alignment mask.
        constexpr uint8_t Sib_Reg_Idx{ 4 };
    }

    void BytecodeCompiler::iaddrs_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        constexpr uint8_t Displacement_Reg_Idx{ 5 };
        const uint8_t dst_register{ instr.dst_register };
        const uint8_t src_register{ instr.src_register };

        reg_usage[dst_register] = idx;
        const auto scale{ 64 * instr.modShift() };
        const auto index{ 8 * src_register };
        const auto base{ dst_register };

        if (dst_register != Displacement_Reg_Idx) {
            const uint32_t lea{ 0x00'04'8d'4f | uint32_t(scale | index | base) << 24 | uint32_t(8 * dst_register) << 16 };
            std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
            code_size += 4;

        } else {
            const uint64_t imm{ instr.imm32 };
            const uint64_t lea{ 0x00'00'00'00'00'ac'8d'4f | uint64_t(scale | index | base) << 24 | imm << 32 };
            std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
            code_size += 8;
        }

    }

    void BytecodeCompiler::iaddm_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register };
        const uint8_t src_register{ instr.src_register };
        reg_usage[dst_register] = idx;

        if (dst_register != src_register) {
            const uint64_t imm{ instr.imm32 };
            const auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

            if (src_register != Sib_Reg_Idx) {
                const uint64_t lea{ 0x48'00'00'00'00'80'8d'49 | uint64_t(src_register) << 16 | uint64_t(imm) << 24 };
                std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
                const uint64_t and_{ 0x04'03'4c'00'00'00'00'25 | uint64_t(mem_mask) << 8 | uint64_t(8 * dst_register) << 56 };
                std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
                const uint8_t mov{ static_cast<uint8_t>(0x06) };
                std::memcpy(code_buffer + code_size + 16, &mov, 1);
                code_size += 17;
            } else {
                const uint64_t lea{ 0x00'00'00'00'24'80'8d'49 | uint64_t(src_register) << 16 | uint64_t(imm) << 32 };
                std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
                const uint64_t and_{ 0x03'4c'00'00'00'00'25'48 | uint64_t(mem_mask) << 16 };
                std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
                const uint16_t mov{ static_cast<uint16_t>(0x06'04 | uint16_t(8 * dst_register)) };
                std::memcpy(code_buffer + code_size + 16, &mov, 2);
                code_size += 18;
            }
        } else {
            const uint64_t offset{ instr.imm32 & Scratchpad_L3_Mask };
            const uint64_t add{ 0x00'00'00'00'00'86'03'4c | uint64_t(8 * dst_register) << 16 | offset << 24 };
            std::memcpy(code_buffer + code_size, &add, sizeof(add));
            code_size += 7;
        }
    }

    void BytecodeCompiler::isubr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register };
        const uint8_t src_register{ instr.src_register };
        reg_usage[dst_register] = idx;

        if (dst_register != src_register) {
            const uint32_t sub{ 0x00'c0'29'4d | uint32_t(dst_register + 8 * src_register) << 16 };
            std::memcpy(code_buffer + code_size, &sub, sizeof(sub));
            code_size += 3;
        } else {
            const uint64_t imm{ instr.imm32 };
            const uint64_t sub{ 0x00'00'00'00'00'e8'81'49 | uint64_t(dst_register) << 16 | imm << 24 };
            std::memcpy(code_buffer + code_size, &sub, sizeof(sub));
            code_size += 7;
        }
    }

    void BytecodeCompiler::isubm_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register };
        const uint8_t src_register{ instr.src_register };
        reg_usage[dst_register] = idx;

        if (dst_register != src_register) {
            const auto imm{ instr.imm32 };
            const auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

            if (src_register != Sib_Reg_Idx) {
                const uint64_t lea{ 0x48'00'00'00'00'80'8d'49 | uint64_t(src_register) << 16 | uint64_t(imm) << 24 };
                std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
                const uint64_t and_{ 0x04'2b'4c'00'00'00'00'25 | uint64_t(mem_mask) << 8 | uint64_t(8 * dst_register) << 56 };
                std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
                const uint8_t mov{ static_cast<uint8_t>(0x06) };
                std::memcpy(code_buffer + code_size + 16, &mov, 1);
                code_size += 17;
            } else {
                const uint64_t lea{ 0x00'00'00'00'24'80'8d'49 | uint64_t(src_register) << 16 | uint64_t(imm) << 32 };
                std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
                const uint64_t and_{ 0x2b'4c'00'00'00'00'25'48 | uint64_t(mem_mask) << 16 };
                std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
                const uint16_t mov{ static_cast<uint16_t>(0x06'04 | uint16_t(8 * dst_register)) };
                std::memcpy(code_buffer + code_size + 16, &mov, 2);
                code_size += 18;
            }
        } else {
            const uint64_t offset{ instr.imm32 & Scratchpad_L3_Mask };
            const uint64_t add{ 0x00'00'00'00'00'86'2b'4c | uint64_t(8 * dst_register) << 16 | offset << 24 };
            std::memcpy(code_buffer + code_size, &add, sizeof(add));
            code_size += 7;
        }
    }

    void BytecodeCompiler::imulr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register };
        const uint8_t src_register{ instr.src_register };

        reg_usage[dst_register] = idx;

        if (dst_register != src_register) {
            const uint32_t imul{ 0xc0'af'0f'4d | uint32_t(8 * dst_register + src_register) << 24 };
            std::memcpy(code_buffer + code_size, &imul, sizeof(imul));
            code_size += 4;
        } else {
            const uint64_t imm{ instr.imm32 };
            const uint64_t imul{ 0x00'00'00'00'00'c0'69'4d | imm << 24 | uint64_t(9 * dst_register) << 16 };
            std::memcpy(code_buffer + code_size, &imul, sizeof(imul));
            code_size += 7;
        }
    }

    void BytecodeCompiler::imulm_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register };
        const uint8_t src_register{ instr.src_register };

        reg_usage[dst_register] = idx;

        if (dst_register != src_register) {
            const auto imm{ instr.imm32 };
            const auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

            if (src_register != Sib_Reg_Idx) {
                const uint64_t lea{ 0x48'00'00'00'00'88'8d'49 | uint64_t(src_register) << 16 | uint64_t(imm) << 24 };
                std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
                const uint64_t and_{ 0x0f'4c'00'00'00'00'e1'81 | uint64_t(mem_mask) << 16 };
                std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
                const uint32_t mov{ uint32_t(0x00'0e'04'af) | uint32_t(8 * dst_register) << 8 };
                std::memcpy(code_buffer + code_size + 16, &mov, sizeof(mov));
                code_size += 19;
            } else {
                const uint64_t lea{ 0x00'00'00'00'24'88'8d'49 | uint64_t(src_register) << 16 | uint64_t(imm) << 32 };
                std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
                const uint64_t and_{ 0x4c'00'00'00'00'e1'81'48 | uint64_t(mem_mask) << 24 };
                std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
                const uint32_t mov{ uint32_t(0x0e'04'af'0f) | uint32_t(8 * dst_register) << 16 };
                std::memcpy(code_buffer + code_size + 16, &mov, sizeof(mov));
                code_size += 20;
            }
        } else {
            const uint64_t offset{ instr.imm32 & Scratchpad_L3_Mask };
            const uint64_t imul{ 0x00'00'00'00'86'af'0f'4c | uint64_t(8 * dst_register) << 24 | offset << 32 };
            std::memcpy(code_buffer + code_size, &imul, sizeof(imul));
            code_size += 8;
        }
    }

    void BytecodeCompiler::imulhr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register };
        const uint8_t src_register{ instr.src_register };
        reg_usage[dst_register] = idx;

        const uint64_t imulhr{ 0x89'49'e0'f7'49'c0'89'4c | uint64_t(src_register) << 40 | uint64_t(8 * dst_register) << 16 };
        std::memcpy(code_buffer + code_size, &imulhr, sizeof(imulhr));
        const uint8_t mov{ static_cast<uint8_t>(0xd0 + dst_register) };
        std::memcpy(code_buffer + code_size + 8, &mov, 1);
        code_size += 9;

    }

    void BytecodeCompiler::imulhm_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register };
        const uint8_t src_register{ instr.src_register };

        reg_usage[dst_register] = idx;

        if (dst_register != src_register) {
            const uint64_t imm{ instr.imm32 };
            const uint64_t mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

            if (src_register != Sib_Reg_Idx) {
                const uint64_t lea{ 0x48'00'00'00'00'88'8d'49 | uint64_t(src_register) << 16 | uint64_t(imm) << 24 };
                std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
                const uint64_t and_{ 0x89'4c'00'00'00'00'e1'81 | uint64_t(mem_mask) << 16 };
                std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
                const uint64_t mov{ 0xd0'89'49'0e'24'f7'48'c0 | uint64_t(dst_register) << 56 | uint64_t(8 * dst_register) };
                std::memcpy(code_buffer + code_size + 16, &mov, sizeof(mov));
                code_size += 24;
            } else {
                const uint64_t lea{ 0x00'00'00'00'24'88'8d'49 | uint64_t(src_register) << 16 | uint64_t(imm) << 32 };
                std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
                const uint64_t and_{ 0x4c'00'00'00'00'e1'81'48 | uint64_t(mem_mask) << 24 };
                std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
                const uint64_t mov{ 0x89'49'0e'24'f7'48'c0'89 | uint64_t(8 * dst_register) << 8 };
                std::memcpy(code_buffer + code_size + 16, &mov, sizeof(mov));
                const uint8_t mov2{ uint8_t(0xd0 + dst_register) };
                std::memcpy(code_buffer + code_size + 24, &mov2, 1);
                code_size += 25;
            }
        } else {
            const uint64_t offset{ instr.imm32 & Scratchpad_L3_Mask };
            const uint64_t mov{ 0x00'00'a6'f7'48'c0'89'4c | uint64_t(8 * dst_register) << 16 | offset << 48 };
            std::memcpy(code_buffer + code_size, &mov, sizeof(mov));
            const uint64_t mulh{ 0x00'00'00'd0'89'49'00'00 | uint64_t(dst_register) << 32 | offset >> 16 };
            std::memcpy(code_buffer + code_size + 8, &mulh, sizeof(mulh));
            code_size += 13;
        }
    }

    void BytecodeCompiler::ismulhr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register };
        const uint8_t src_register{ instr.src_register };

        reg_usage[dst_register] = idx;

        const uint64_t imulhr{ 0x89'49'e8'f7'49'c0'89'4c | uint64_t(src_register) << 40 | uint64_t(8 * dst_register) << 16 };
        std::memcpy(code_buffer + code_size, &imulhr, sizeof(imulhr));
        const uint8_t mov{ static_cast<uint8_t>(0xd0 + dst_register) };
        std::memcpy(code_buffer + code_size + 8, &mov, 1);
        code_size += 9;
    }

    void BytecodeCompiler::ismulhm_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register };
        const uint8_t src_register{ instr.src_register };
        reg_usage[dst_register] = idx;

        if (dst_register != src_register) {
            const uint64_t imm{ instr.imm32 };
            const uint64_t mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

            if (src_register != Sib_Reg_Idx) {
                const uint64_t lea{ 0x48'00'00'00'00'88'8d'49 | uint64_t(src_register) << 16 | uint64_t(imm) << 24 };
                std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
                const uint64_t and_{ 0x89'4c'00'00'00'00'e1'81 | uint64_t(mem_mask) << 16 };
                std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
                const uint64_t mov{ 0xd0'89'49'0e'2c'f7'48'c0 | uint64_t(dst_register) << 56 | uint64_t(8 * dst_register) };
                std::memcpy(code_buffer + code_size + 16, &mov, sizeof(mov));
                code_size += 24;
            } else {
                const uint64_t lea{ 0x00'00'00'00'24'88'8d'49 | uint64_t(src_register) << 16 | uint64_t(imm) << 32 };
                std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
                const uint64_t and_{ 0x4c'00'00'00'00'e1'81'48 | uint64_t(mem_mask) << 24 };
                std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
                const uint64_t mov{ 0x89'49'0e'2c'f7'48'c0'89 | uint64_t(8 * dst_register) << 8 };
                std::memcpy(code_buffer + code_size + 16, &mov, sizeof(mov));
                const uint8_t mov2{ uint8_t(0xd0 + dst_register) };
                std::memcpy(code_buffer + code_size + 24, &mov2, 1);
                code_size += 25;
            }
        } else {
            const uint64_t offset{ instr.imm32 & Scratchpad_L3_Mask };

            const uint64_t mov{ 0x00'00'ae'f7'48'c0'89'4c | uint64_t(8 * dst_register) << 16 | offset << 48 };
            std::memcpy(code_buffer + code_size, &mov, sizeof(mov));
            const uint64_t mulh{ 0x00'00'00'd0'89'49'00'00 | uint64_t(dst_register) << 32 | offset >> 16 };
            std::memcpy(code_buffer + code_size + 8, &mulh, sizeof(mulh));
            code_size += 13;
        }
    }

    void BytecodeCompiler::inegr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register };
        reg_usage[dst_register] = idx;
        const uint32_t neg{ 0x00'd8'f7'49 | uint32_t(dst_register) << 16 };
        std::memcpy(code_buffer + code_size, &neg, sizeof(neg));
        code_size += 3;
    }

    void BytecodeCompiler::ixorr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register };
        const uint8_t src_register{ instr.src_register };
        reg_usage[dst_register] = idx;

        if (dst_register != src_register) {
            const uint32_t xor_{ 0x00'c0'31'4d | uint32_t(dst_register + 8 * src_register) << 16 };
            std::memcpy(code_buffer + code_size, &xor_, sizeof(xor_));
            code_size += 3;
        } else {
            const uint64_t imm{ instr.imm32 };
            const uint64_t xor_{ 0x00'00'00'00'00'f0'81'49 | uint64_t(dst_register) << 16 | imm << 24 };
            std::memcpy(code_buffer + code_size, &xor_, sizeof(xor_));
            code_size += 7;
        }
    }

    void BytecodeCompiler::ixorm_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register };
        const uint8_t src_register{ instr.src_register };

        reg_usage[dst_register] = idx;

        if (dst_register != src_register) {
            const uint64_t imm{ instr.imm32 };
            const uint64_t mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };

            if (src_register != Sib_Reg_Idx) {
                const uint64_t lea{ 0x48'00'00'00'00'80'8d'49 | uint64_t(src_register) << 16 | uint64_t(imm) << 24 };
                std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
                const uint64_t and_{ 0x04'33'4c'00'00'00'00'25 | uint64_t(mem_mask) << 8 | uint64_t(8 * dst_register) << 56 };
                std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
                const uint8_t mov{ static_cast<uint8_t>(0x06) };
                std::memcpy(code_buffer + code_size + 16, &mov, 1);
                code_size += 17;
            } else {
                const uint64_t lea{ 0x00'00'00'00'24'80'8d'49 | uint64_t(src_register) << 16 | uint64_t(imm) << 32 };
                std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
                const uint64_t and_{ 0x33'4c'00'00'00'00'25'48 | uint64_t(mem_mask) << 16 };
                std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
                const uint16_t mov{ static_cast<uint16_t>(0x06'04 | uint16_t(8 * dst_register)) };
                std::memcpy(code_buffer + code_size + 16, &mov, 2);
                code_size += 18;
            }
        } else {
            const uint64_t offset{ instr.imm32 & Scratchpad_L3_Mask };
            const uint64_t xor_{ 0x00'00'00'00'00'86'33'4c | uint64_t(8 * dst_register) << 16 | offset << 24 };
            std::memcpy(code_buffer + code_size, &xor_, sizeof(xor_));
            code_size += 7;
        }
    }

    void BytecodeCompiler::irorr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register };
        const uint8_t src_register{ instr.src_register };

        // Set even for rotate == 0.
        reg_usage[dst_register] = idx;

        if (dst_register != src_register) {
            const uint64_t ror{ 0x00'00'c8'd3'49'c1'89'4c | uint64_t(8 * src_register) << 16 | uint64_t(dst_register) << 40 };
            std::memcpy(code_buffer + code_size, &ror, sizeof(ror));
            code_size += 6;
        } else if (const uint32_t imm = instr.imm32 % 64; imm != 0) {
            const uint32_t ror{ 0x00'c8'c1'49 | uint32_t(dst_register) << 16 | imm << 24 };
            std::memcpy(code_buffer + code_size, &ror, sizeof(ror));
            code_size += 4;
        }
    }

    void BytecodeCompiler::irolr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register };
        const uint8_t src_register{ instr.src_register };
        // Set even for rotate == 0.
        reg_usage[dst_register] = idx;

        if (dst_register != src_register) {
            const uint64_t rol{ 0x00'00'c0'd3'49'c1'89'4c | uint64_t(8 * src_register) << 16 | uint64_t(dst_register) << 40 };
            std::memcpy(code_buffer + code_size, &rol, sizeof(rol));
            code_size += 6;
        } else if (const uint32_t imm = instr.imm32 % 64; imm != 0) {
            const uint32_t rol{ 0x00'c0'c1'49 | uint32_t(dst_register) << 16 | imm << 24 };
            std::memcpy(code_buffer + code_size, &rol, sizeof(rol));
            code_size += 4;
        }
    }

    void BytecodeCompiler::imulrcp_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        if (const uint32_t imm = instr.imm32; imm != 0 && !std::has_single_bit(imm)) {
            const uint8_t dst_register{ instr.dst_register };
            reg_usage[dst_register] = idx;
            const auto rcp{ reciprocal(imm) };

            const uint64_t mov{ 0x00'00'00'00'00'00'b8'48 | rcp << 16 };
            std::memcpy(code_buffer + code_size, &mov, sizeof(mov));
            const uint64_t imul{ 0x00'00'c0'af'0f'4c'00'00 | rcp >> 48 | uint64_t(8 * dst_register) << 40 };
            std::memcpy(code_buffer + code_size + 8, &imul, sizeof(imul));
            code_size += 14;
        }
    }

    void BytecodeCompiler::iswapr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register };
        const uint8_t src_register{ instr.src_register };

        if (src_register != dst_register) {
            reg_usage[src_register] = idx;
            reg_usage[dst_register] = idx;

            const uint32_t xchg{ 0x00'c0'87'4d | uint32_t(dst_register + 8 * src_register) << 16 };
            std::memcpy(code_buffer + code_size, &xchg, sizeof(xchg));
            code_size += 3;
        }
    }

    void BytecodeCompiler::cbranch_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register };
        const uint8_t src_register{ instr.src_register };

        constexpr uint32_t Condition_Mask{ (1 << Rx_Jump_Bits) - 1 };
        const auto shift{ instr.modCond() + Rx_Jump_Offset };
        const auto mem_mask{ Condition_Mask << shift };

        static_assert(Rx_Jump_Offset > 0, "Below simplification requires this assertion");
        uint32_t imm{ instr.imm32 | (1 << shift) };
        imm &= ~(1ULL << (shift - 1)); // Clear the bit below the condition mask - this limits the number of successive jumps to 2.

        const auto jmp_target{ reg_usage[dst_register] + 1 };
        const int32_t jmp_offset{ instr_offset[jmp_target] - instr_offset[idx] - 20 };

        const uint64_t add{ 0x49'00'00'00'00'c0'81'49 | uint64_t(dst_register) << 16 | uint64_t(imm) << 24 };
        std::memcpy(code_buffer + code_size, &add, sizeof(add));
        const uint64_t test{ 0x84'0f'00'00'00'00'c0'f7 | uint64_t(dst_register) << 8 | uint64_t(mem_mask) << 16 };
        std::memcpy(code_buffer + code_size + 8, &test, sizeof(test));
        std::memcpy(code_buffer + code_size + 16, &jmp_offset, sizeof(jmp_offset));
        code_size += 20;

        for (int i = 0; i < Int_Register_Count; ++i) {
            reg_usage[i] = idx; // Set all registers as used.
        }
    }

    void BytecodeCompiler::fswapr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register };
        const uint64_t vshufpd{ 0x00'00'00'01'c0'c6'c1'c5 | uint64_t(56 - 8 * dst_register) << 8 | uint64_t(9 * dst_register) << 24 };
        std::memcpy(code_buffer + code_size, &vshufpd, sizeof(vshufpd));
        code_size += 5;
    }

    void BytecodeCompiler::faddr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register % Float_Register_Count };
        const uint8_t src_register{ instr.src_register % Float_Register_Count };
        const uint32_t vaddpd{ 0xc0'58'a1'c5 | uint32_t(24 - 8 * src_register) << 8 | uint32_t(9 * dst_register) << 24 };
        std::memcpy(code_buffer + code_size, &vaddpd, sizeof(vaddpd));
        code_size += 4;
    }

    void BytecodeCompiler::faddm_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t src_register{ instr.src_register };
        const uint8_t f_dst_register{ instr.dst_register % Float_Register_Count };
        const uint64_t mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };
        const uint64_t imm{ instr.imm32 };

        if (src_register != Sib_Reg_Idx) {
            const uint64_t lea{ 0x48'00'00'00'00'80'8d'49 | uint64_t(src_register) << 16 | uint64_t(imm) << 24 };
            std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
            const uint64_t and_{ 0xe6'7a'c5'00'00'00'00'25 | uint64_t(mem_mask) << 8 };
            std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
            const uint64_t mov{ 0x00'00'c0'58'99'c5'06'24 | uint64_t(9 * f_dst_register) << 40 };
            std::memcpy(code_buffer + code_size + 16, &mov, sizeof(mov));
            code_size += 22;
        } else {
            const uint64_t lea{ 0x00'00'00'00'24'80'8d'49 | uint64_t(src_register) << 16 | uint64_t(imm) << 32 };
            std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
            const uint64_t and_{ 0x7a'c5'00'00'00'00'25'48 | uint64_t(mem_mask) << 16 };
            std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
            const uint64_t mov{ 0x00'c0'58'99'c5'06'24'e6 | uint64_t(9 * f_dst_register) << 48 };
            std::memcpy(code_buffer + code_size + 16, &mov, sizeof(mov));
            code_size += 23;
        }
    }

    void BytecodeCompiler::fsubr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register % Float_Register_Count };
        const uint8_t src_register{ instr.src_register % Float_Register_Count };
        const uint64_t vsubpd{ 0x00'00'00'c0'5c'61'c1'c4 | uint64_t(24 - 8 * dst_register) << 16 | uint64_t(8 * dst_register + src_register) << 32 };
        std::memcpy(code_buffer + code_size, &vsubpd, sizeof(vsubpd));
        code_size += 5;
    }

    void BytecodeCompiler::fsubm_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t src_register{ instr.src_register };
        const uint8_t f_dst_register{ instr.dst_register % Float_Register_Count };
        const uint64_t mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };
        const uint64_t imm{ instr.imm32 };

        if (src_register != Sib_Reg_Idx) {
            const uint64_t lea{ 0x48'00'00'00'00'80'8d'49 | uint64_t(src_register) << 16 | uint64_t(imm) << 24 };
            std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
            const uint64_t and_{ 0xe6'7a'c5'00'00'00'00'25 | uint64_t(mem_mask) << 8 };
            std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
            const uint64_t mov{ 0x00'c4'5c'61'c1'c4'06'24 | uint64_t(8 * f_dst_register) << 48 | uint64_t(24 - 8 * f_dst_register) << 32 };
            std::memcpy(code_buffer + code_size + 16, &mov, sizeof(mov));
            code_size += 23;
        } else {
            const uint64_t lea{ 0x00'00'00'00'24'80'8d'49 | uint64_t(src_register) << 16 | uint64_t(imm) << 32 };
            std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
            const uint64_t and_{ 0x7a'c5'00'00'00'00'25'48 | uint64_t(mem_mask) << 16 };
            std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
            const uint64_t mov{ 0xc4'5c'61'c1'c4'06'24'e6 | uint64_t(8 * f_dst_register) << 56 | uint64_t(24 - 8 * f_dst_register) << 40 };
            std::memcpy(code_buffer + code_size + 16, &mov, sizeof(mov));
            code_size += 24;
        }
    }

    void BytecodeCompiler::fscalr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register % Float_Register_Count };
        const uint32_t xorps{ 0xc6'57'0f'41 | uint32_t(8 * dst_register) << 24 };
        std::memcpy(code_buffer + code_size, &xorps, sizeof(xorps));
        code_size += 4;
    }

    void BytecodeCompiler::fmulr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register % Float_Register_Count };
        const uint8_t src_register{ instr.src_register % Float_Register_Count };
        const uint32_t vmulpd{ 0xe4'59'81'c5 | uint32_t(56 - 8 * src_register) << 8 | uint32_t(9 * dst_register) << 24 };
        std::memcpy(code_buffer + code_size, &vmulpd, sizeof(vmulpd));
        code_size += 4;
    }

    void BytecodeCompiler::fdivm_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t src_register{ instr.src_register };
        const uint8_t f_dst_register{ instr.dst_register % Float_Register_Count };
        const uint64_t mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };
        const uint64_t imm{ instr.imm32 };

        if (src_register != Sib_Reg_Idx) {
            const uint64_t lea{ 0x48'00'00'00'00'80'8d'49 | uint64_t(src_register) << 16 | uint64_t(imm) << 24 };
            std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
            const uint64_t and_{ 0xe6'7a'c5'00'00'00'00'25 | uint64_t(mem_mask) << 8 };
            std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
            const uint64_t andps{ 0x0f'45'e7'54'0f'45'06'24 };
            std::memcpy(code_buffer + code_size + 16, &andps, sizeof(andps));
            const uint64_t orps{ 0x00'e4'5e'41'c1'c4'e5'56 | uint64_t(8 * f_dst_register) << 48 | uint64_t(24 - 8 * f_dst_register) << 32 };
            std::memcpy(code_buffer + code_size + 24, &orps, sizeof(orps));
            code_size += 31;

        } else {
            const uint64_t lea{ 0x00'00'00'00'24'80'8d'49 | uint64_t(src_register) << 16 | uint64_t(imm) << 32 };
            std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
            const uint64_t and_{ 0x7a'c5'00'00'00'00'25'48 | uint64_t(mem_mask) << 16 };
            std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
            const uint64_t andps{ 0x45'e7'54'0f'45'06'24'e6 };
            std::memcpy(code_buffer + code_size + 16, &andps, sizeof(andps));
            const uint64_t orps{ 0xe4'5e'41'c1'c4'e5'56'0f | uint64_t(8 * f_dst_register) << 56 | uint64_t(24 - 8 * f_dst_register) << 40 };
            std::memcpy(code_buffer + code_size + 24, &orps, sizeof(orps));
            code_size += 32;
        }
    }

    void BytecodeCompiler::fsqrtr_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register % Float_Register_Count };
        const uint32_t vsqrtpd{ 0xe4'51'f9'c5 | uint32_t(9 * dst_register) << 24 };
        std::memcpy(code_buffer + code_size, &vsqrtpd, sizeof(vsqrtpd));
        code_size += 4;
    }

    void BytecodeCompiler::cfround_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t src_register{ instr.src_register };

        if (const uint64_t imm{ instr.imm32 % 64 }; imm != 0) {
            const uint64_t mov{ 0x48'00'c8'c1'48'c0'89'4c | imm << 48 | uint64_t(8 * src_register) << 16 };
            std::memcpy(code_buffer + code_size, &mov, sizeof(mov));
            const uint64_t and_{ 0x48'0d'c0'c1'48'03'e0'83 };
            std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
            const uint64_t or_{ 0xae'0f'50'00'00'9f'c0'0d };
            std::memcpy(code_buffer + code_size + 16, &or_, sizeof(or_));
            const uint32_t ldmxcsr{ 0x00'58'24'14 };
            std::memcpy(code_buffer + code_size + 24, &ldmxcsr, sizeof(ldmxcsr));
            code_size += 27;
        } else {
            const uint64_t mov{ 0x48'03'e0'83'48'c0'89'4c | uint64_t(8 * src_register) << 16 };
            std::memcpy(code_buffer + code_size, &mov, sizeof(mov));
            const uint64_t rol{ 0x00'9f'c0'0d'48'0d'c0'c1 };
            std::memcpy(code_buffer + code_size + 8, &rol, sizeof(rol));
            const uint64_t or_{ 0x00'58'24'14'ae'0f'50'00 };
            std::memcpy(code_buffer + code_size + 16, &or_, sizeof(or_));
            code_size += 23;
        }
    }

    void BytecodeCompiler::istore_cmpl(const RxInstruction& instr, const uint32_t idx) noexcept {
        const uint8_t dst_register{ instr.dst_register };
        const uint8_t src_register{ instr.src_register };
        const uint32_t imm{ instr.imm32 };
        constexpr uint32_t L3_Store_Condition{ 14 };

        auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };
        if (instr.modCond() >= L3_Store_Condition) {
            mem_mask = Scratchpad_L3_Mask;
        }

        if (dst_register != Sib_Reg_Idx) {
            const uint64_t lea{ 0x48'00'00'00'00'80'8d'49 | uint64_t(dst_register) << 16 | uint64_t(imm) << 24 };
            std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
            const uint64_t and_{ 0x04'89'4c'00'00'00'00'25 | uint64_t(mem_mask) << 8 | uint64_t(8 * src_register) << 56 };
            std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
            const uint8_t mov{ static_cast<uint8_t>(0x06) };
            std::memcpy(code_buffer + code_size + 16, &mov, 1);
            code_size += 17;
        } else {
            const uint64_t lea{ 0x00'00'00'00'24'80'8d'49 | uint64_t(dst_register) << 16 | uint64_t(imm) << 32 };
            std::memcpy(code_buffer + code_size, &lea, sizeof(lea));
            const uint64_t and_{ 0x89'4c'00'00'00'00'25'48 | uint64_t(mem_mask) << 16 };
            std::memcpy(code_buffer + code_size + 8, &and_, sizeof(and_));
            const uint16_t mov{ static_cast<uint16_t>(0x06'04 | uint16_t(8 * src_register)) };
            std::memcpy(code_buffer + code_size + 16, &mov, 2);
            code_size += 18;
        }
    }
}

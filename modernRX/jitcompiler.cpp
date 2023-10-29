#include "assembler.hpp"
#include "jitcompiler.hpp"
#include "superscalar.hpp"


namespace modernRX {
    namespace {
        [[nodiscard]] void emitAVX2Instruction(assembler::Context& asmb, int32_t& data_offset, const SuperscalarInstruction& instr);
    }

    [[nodiscard]] jit_function_ptr<JITDatasetItemProgram> compile(const_span<SuperscalarProgram, Rx_Cache_Accesses> programs) {
        using namespace assembler::registers;
        using namespace assembler;
        assembler::Context asmb(128 * 1024, 32 * 1024);

        // I. Prolog
        // 1) Push registers to stack and align it to 64 bytes boundary.
        asmb.push(RBX, RSI, RDI, R13, R14, R15, XMM6, XMM7, XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15);
        asmb.alignStack();
        // 2) Declare local variable (in order: cache_indexes, cache_item_mask, vpmulhuq mask, vpmullq mask, v4q_add_consts, v4q_item_numbers_step).
        asmb.sub(RSP, 0xC0); // 6x ymm registers
        // 3) Move data ptr to RBX.
        asmb.movDataPtr(RBX);
        // 4) Prepare const values used in step 1 of: https://github.com/tevador/RandomX/blob/master/doc/specs.md#73-dataset-block-generation
        const auto v4q_item_numbers_add{ asmb.storeVector4q<uint64_t, RBX>(1, 2, 3, 4) }; // item_number adder - RBX[0]
        const auto v4q_mul_consts{ asmb.storeImmediate<uint64_t, Register::YMM(0).size(), RBX>(6364136223846793005ULL) }; // mul_consts - RBX[32]
        asmb.storeImmediate<uint64_t, Register::YMM(0).size()>(9298411001130361340ULL); // RBX[64]
        asmb.storeImmediate<uint64_t, Register::YMM(0).size()>(12065312585734608966ULL); // RBX[96]
        asmb.storeImmediate<uint64_t, Register::YMM(0).size()>(9306329213124626780ULL); // RBX[128]
        asmb.storeImmediate<uint64_t, Register::YMM(0).size()>(5281919268842080866ULL); // RBX[160]
        asmb.storeImmediate<uint64_t, Register::YMM(0).size()>(10536153434571861004ULL); // RBX[192]
        asmb.storeImmediate<uint64_t, Register::YMM(0).size()>(3398623926847679864ULL); // RBX[224]
        asmb.storeImmediate<uint64_t, Register::YMM(0).size()>(9549104520008361294ULL); // RBX[256]

        // 5) Set buffer ptr in R10. [RCX] + 128 - pointer in span. Move pointer by 128 bytes to reduce total code size when storing dataset items.
        asmb.mov(R10, RCX[0]);
        asmb.add(R10, 128);
        // 6) Set buffer size in R13. [RCX + 8] - size of submemory span.
        //    Divide size by batch size (4) - this register is loop counter.
        asmb.mov(R13, RCX[8]);
        asmb.shr(R13, 2);
        // 7) Move cache ptr to YMM6. This register will never be used for anything else.
        asmb.vpbroadcastq(YMM6, RDX);
        const auto& cache_ptr_reg{ YMM6 };

        // 9) Initialize local variables
        const auto cache_item_mask{ asmb.put4qVectorOnStack(R08, 32) };
        asmb.put4qVectorOnStack(0x00000000ffffffff, 64); // vpmulhuq mask
        const auto v4q_mullq_mask{ asmb.put4qVectorOnStack(0xffffffff00000000, 96) };
        const auto v4q_add_consts{ asmb.put4qVectorOnStack(7009800821677620404ULL, 128) }; // This value is v4q_mul_consts multiplied by 4.
        const auto v4q_item_numbers_step{ asmb.put4qVectorOnStack(4, 160) };
        asmb.vpbroadcastq(YMM5, R09);
        asmb.vonereg(YMM4);
        asmb.vpaddq(YMM5, YMM5, v4q_item_numbers_add);
        asmb.vpsubq(YMM3, YMM5, YMM4);
        asmb.vmovdqa(RSP[0], YMM3);
        const auto& cache_indexes_stack{ RSP[0] };
        const auto& cache_indexes_reg{ YMM3 };

        // 10) Set initial ymmitem0 in YMM7. ymmitem0 = (v4q(start_item) + v4q_item_numbers_add) * v4q_mul_consts
        // YMM7 will never be used for anything else.
        asmb.vmovdqa(YMM4, v4q_mullq_mask);
        asmb.vpmullq(YMM7, YMM5, v4q_mul_consts);
        const auto& ymmitem0{ YMM7 };

        // II. Main loop
        // 11) Start loop over all elements.
        asmb.label("loop");

        // 12) Set data pointer to proper offset.
        asmb.movDataPtr(RBX, 192);

        // 13) Set initial dataset item values.
        // Uses "register-wise" layout:
        // A0 B0 C0 D0
        // A1 B1 C1 D1
        // ...
        // A7 B7 C7 D7
        asmb.vmovdqa(YMM8, YMM7);
        asmb.vpxor(YMM9, YMM7, RBX[-128]);
        asmb.vpxor(YMM10, YMM7, RBX[-96]);
        asmb.vpxor(YMM11, YMM7, RBX[-64]);
        asmb.vpxor(YMM12, YMM7, RBX[-32]);
        asmb.vpxor(YMM13, YMM7, RBX[0]);
        asmb.vpxor(YMM14, YMM7, RBX[32]);
        asmb.vpxor(YMM15, YMM7, RBX[64]);
        int32_t data_offset{ 96 };

        // 14) Execute all programs.
        for (uint32_t i = 0; i < 8; ++i) {
            const auto& program{ programs[i] };

            // 15) Prepare registers for program execution.
            asmb.vmovdqa(YMM4, v4q_mullq_mask); // Set vpmullq mask in YMM4.
            asmb.vzeroreg(YMM5); // Zero out YMM5.

            // 16) Set cache item indexes.
            if (i == 0) {
                asmb.vpand(YMM0, cache_indexes_reg, cache_item_mask);
            } else {
                // For programs 1-7 address register of the previous program is used.
                const auto cache_indexes_reg_tmp{ Register::YMM(8 | programs[i - 1].address_register) };
                asmb.vpand(YMM0, cache_indexes_reg_tmp, cache_item_mask);
            }

            // 17) Set cache item pointers.
            static_assert(sizeof(DatasetItem) == 64);
            asmb.vpsllq(YMM0, YMM0, static_cast<int>(std::log2(sizeof(DatasetItem)))); // Shift by 6.
            asmb.vpaddq(YMM0, YMM0, cache_ptr_reg);

            // 18) Prefetch cache items.
            asmb.vextracti128(XMM1, YMM0, 1);
            asmb.vpextrq(R14, XMM0, 1);
            asmb.prefetchnta(R14[0]);
            asmb.vmovq(RSI, XMM0);
            asmb.prefetchnta(RSI[0]);
            asmb.vmovq(RDI, XMM1);
            asmb.prefetchnta(RDI[0]);
            asmb.vpextrq(R15, XMM1, 1);
            asmb.prefetchnta(R15[0]);

            // 19) Execute every single instruction of program.
            for (uint32_t j = 0; j < program.size; ++j) {
                const SuperscalarInstruction& instr{ program.instructions[j] };
                emitAVX2Instruction(asmb, data_offset, instr);
            }

            // 20) Transpose forth and back registers 0-3 and perform XOR with cache items.
            asmb.vpunpcklqdq(YMM0, YMM8, YMM9);         // A0 A1 C0 C1
            asmb.vpunpcklqdq(YMM1, YMM10, YMM11);       // A2 A3 C2 C3
            asmb.vperm2i128(YMM2, YMM0, YMM1, 0x20);    // A0 A1 A2 A3
            asmb.vperm2i128(YMM3, YMM0, YMM1, 0x31);    // C0 C1 C2 C3

            asmb.vpunpckhqdq(YMM0, YMM8, YMM9);         // B0 B1 D0 D1
            asmb.vpunpckhqdq(YMM1, YMM10, YMM11);       // B2 B3 D2 D3
            asmb.vperm2i128(YMM4, YMM0, YMM1, 0x20);    // B0 B1 B2 B3
            asmb.vperm2i128(YMM5, YMM0, YMM1, 0x31);    // D0 D1 D2 D3

            // 21) If this is the last program do not transpose registers.
            if (i == programs.size() - 1) {
                asmb.vpxor(YMM8, YMM2, RSI[0]);
                asmb.vpxor(YMM10, YMM3, RDI[0]);
                asmb.vpxor(YMM9, YMM4, R14[0]);
                asmb.vpxor(YMM11, YMM5, R15[0]);
            } else {
                asmb.vpxor(YMM2, YMM2, RSI[0]);
                asmb.vpxor(YMM3, YMM3, RDI[0]);
                asmb.vpxor(YMM4, YMM4, R14[0]);
                asmb.vpxor(YMM5, YMM5, R15[0]);

                asmb.vpunpcklqdq(YMM0, YMM2, YMM4);         // A0 B0 A2 B2
                asmb.vpunpcklqdq(YMM1, YMM3, YMM5);         // C0 D0 C2 D2
                asmb.vperm2i128(YMM8, YMM0, YMM1, 0x20);    // A0 B0 C0 D0
                asmb.vperm2i128(YMM10, YMM0, YMM1, 0x31);   // A2 B2 C2 D2

                asmb.vpunpckhqdq(YMM0, YMM2, YMM4);         // A1 B1 A3 B3
                asmb.vpunpckhqdq(YMM1, YMM3, YMM5);         // C1 D1 C3 D3
                asmb.vperm2i128(YMM9, YMM0, YMM1, 0x20);    // A1 B1 C1 D1
                asmb.vperm2i128(YMM11, YMM0, YMM1, 0x31);   // A3 B3 C3 D3
            }

            // 22) Transpose forth and back registers 4-7 and perform XOR with cache items.
            asmb.vpunpcklqdq(YMM0, YMM12, YMM13);        // A4 A5 C4 C5
            asmb.vpunpcklqdq(YMM1, YMM14, YMM15);        // A6 A7 C6 C7
            asmb.vperm2i128(YMM2, YMM0, YMM1, 0x20);     // A4 A5 A6 A7
            asmb.vperm2i128(YMM3, YMM0, YMM1, 0x31);     // C4 C5 C6 C7

            asmb.vpunpckhqdq(YMM0, YMM12, YMM13);        // B4 B5 D4 D5
            asmb.vpunpckhqdq(YMM1, YMM14, YMM15);        // B6 B7 D6 D7
            asmb.vperm2i128(YMM4, YMM0, YMM1, 0x20);     // B4 B5 B6 B7
            asmb.vperm2i128(YMM5, YMM0, YMM1, 0x31);     // D4 D5 D6 D7


            // 23) If this is the last program do not transpose registers.
            if (i == programs.size() - 1) {
                asmb.vpxor(YMM12, YMM2, RSI[32]);
                asmb.vpxor(YMM14, YMM3, RDI[32]);
                asmb.vpxor(YMM13, YMM4, R14[32]);
                asmb.vpxor(YMM15, YMM5, R15[32]);
            } else {
                asmb.vpxor(YMM2, YMM2, RSI[32]);
                asmb.vpxor(YMM3, YMM3, RDI[32]);
                asmb.vpxor(YMM4, YMM4, R14[32]);
                asmb.vpxor(YMM5, YMM5, R15[32]);

                asmb.vpunpcklqdq(YMM0, YMM2, YMM4);        // A4 B4 A6 B6    
                asmb.vpunpcklqdq(YMM1, YMM3, YMM5);        // C4 D4 C6 D6
                asmb.vperm2i128(YMM12, YMM0, YMM1, 0x20);  // A4 B4 C4 D4
                asmb.vperm2i128(YMM14, YMM0, YMM1, 0x31);  // A6 B6 C6 D6

                asmb.vpunpckhqdq(YMM0, YMM2, YMM4);        // A5 B5 A7 B7
                asmb.vpunpckhqdq(YMM1, YMM3, YMM5);        // C5 D5 C7 D7
                asmb.vperm2i128(YMM13, YMM0, YMM1, 0x20);  // A5 B5 C5 D5
                asmb.vperm2i128(YMM15, YMM0, YMM1, 0x31);  // A7 B7 C7 D7
            }
        }

        // 24) Prepare ymmitem0 for next iteration. ymmitem0 = ymmitem0 + v4q_add_consts
        asmb.vpaddq(YMM7, YMM7, v4q_add_consts);

        // 25) Prepare cache_indexes for next iteration. cache_indexes = cache_indexes + v4q_item_numbers_step
        asmb.vmovdqa(cache_indexes_reg, cache_indexes_stack);
        asmb.vpaddq(cache_indexes_reg, cache_indexes_reg, v4q_item_numbers_step);
        asmb.vmovdqa(cache_indexes_stack, cache_indexes_reg);

        // 26) Store dataset items to memory.
        asmb.vmovntdq(R10[-128], YMM8);
        asmb.vmovntdq(R10[-96], YMM12);
        asmb.vmovntdq(R10[-64], YMM9);
        asmb.vmovntdq(R10[-32], YMM13);
        asmb.vmovntdq(R10[0], YMM10);
        asmb.vmovntdq(R10[32], YMM14);
        asmb.vmovntdq(R10[64], YMM11);
        asmb.vmovntdq(R10[96], YMM15);

        // III. End of loop.
        // 27) Update dataset pointer.
        asmb.add(R10, 256);

        // 28) Decrease loop counter.
        asmb.sub(R13, 1);
        asmb.jne("loop");

        // IV. Epilogue
        // 29) Destroy local variable (cache_indexes, cache_item_mask, vpmulhuq mask, vpmullq mask, v4q_add_consts, v4q_item_numbers_step).
        asmb.add(RSP, 0xC0);

        // 30) Unalign stack.
        asmb.unalignStack();

        // 31) Pop registers from stack.
        asmb.pop(XMM15, XMM14, XMM13, XMM12, XMM11, XMM10, XMM9, XMM8, XMM7, XMM6, R15, R14, R13, RDI, RSI, RBX);

        // 32) Zero out upper 128 bits of all YMM registers.
        asmb.vzeroupper();

        // 33) Return.
        asmb.ret();

        // Make the compiled code executable and store a pointer to it in the program.
        // Give away ownership of the code and data to the program.
        return makeExecutable<JITDatasetItemProgram>(asmb.flushCode(), asmb.flushData());
    }

    namespace {
        // Translates every single superscalar instruction into native code using AVX2.
        // data_offset is used to track data section offset. If 256 bytes of data section is used, data pointer in RBX is moved to next 256 bytes. This is for reducing total code size.
        void emitAVX2Instruction(assembler::Context& asmb, int32_t& data_offset, const SuperscalarInstruction& instr) {
            using namespace assembler::registers;
            using namespace assembler;

            const Register dst{ Register::YMM(instr.dst_register | 8) };
            Register src{ Register::YMM(instr.src_register.has_value() ? instr.src_register.value() | 8 : static_cast<reg_idx_t>(0)) };

            switch (instr.type()) {
            case SuperscalarInstructionType::IADD_C7: [[fallthrough]];
            case SuperscalarInstructionType::IADD_C8: [[fallthrough]];
            case SuperscalarInstructionType::IADD_C9:
            {
                asmb.storeImmediate<int64_t, Register::YMM(0).size()>(static_cast<int64_t>(static_cast<int32_t>(instr.imm32))); // Store 2's complement of immediate value.
                asmb.vpaddq(dst, dst, RBX[data_offset]);
                data_offset += 32;
                break;
            }
            case SuperscalarInstructionType::IXOR_C7: [[fallthrough]];
            case SuperscalarInstructionType::IXOR_C8: [[fallthrough]];
            case SuperscalarInstructionType::IXOR_C9:
            {
                asmb.storeImmediate<int64_t, Register::YMM(0).size()>(static_cast<int64_t>(static_cast<int32_t>(instr.imm32))); // Store 2's complement of immediate value.
                asmb.vpxor(dst, dst, RBX[data_offset]);
                data_offset += 32;
                break;
            }
            case SuperscalarInstructionType::IADD_RS:
                if (instr.modShift() > 0) {
                    asmb.vpsllq(YMM0, src, instr.modShift());
                    src = YMM0;
                }

                asmb.vpaddq(dst, dst, src);
                break;
            case SuperscalarInstructionType::ISUB_R:
                asmb.vpsubq(dst, dst, src);
                break;
            case SuperscalarInstructionType::IXOR_R:
                asmb.vpxor(dst, dst, src);
                break;
            case SuperscalarInstructionType::IROR_C:
                asmb.vpsrlq(YMM0, dst, instr.imm32);
                asmb.vpsllq(dst, dst, 64 - instr.imm32);
                asmb.vpor(dst, dst, YMM0);
                break;
            case SuperscalarInstructionType::IMUL_R:
                asmb.vpmullq(dst, dst, src);
                break;
            case SuperscalarInstructionType::ISMULH_R:
                asmb.vpmulhq(dst, src);
                break;
            case SuperscalarInstructionType::IMULH_R:
                asmb.vpmulhuq(dst, dst, src);
                break;
            case SuperscalarInstructionType::IMUL_RCP:
            {
                asmb.storeImmediate<uint64_t, Register::YMM(0).size()>(instr.reciprocal);
                asmb.vpmullq(dst, dst, RBX[data_offset]);
                data_offset += 32;
                break;
            }
            default:
                std::unreachable();
            }

            if (data_offset == 128) {
                data_offset = -128;
                asmb.add(RBX, 256);
            }
        }
    }
}

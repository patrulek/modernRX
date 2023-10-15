#include <cmath>

#include "assembler.hpp"
#include "jitcompiler.hpp"
#include "superscalar.hpp"


namespace modernRX {
	namespace {
		[[nodiscard]] void emitAVX2Instruction(assembler::Context& asmb, const SuperscalarInstruction& instr) noexcept;
	}

	jit_function_ptr<JITDatasetItemProgram> compile(const_span<SuperscalarProgram, Rx_Cache_Accesses> programs) {
		using namespace assembler::registers;
		using namespace assembler;
		assembler::Context asmb;

		// I. Prolog
		// 1) Push registers to stack.
		asmb.push(RAX, RBX, RCX, RDX, RSI, RDI, R10, R13, R14, R15, YMM0, YMM1, YMM2, YMM3, YMM4, YMM5, YMM6, YMM7, YMM8, YMM9, YMM10, YMM11, YMM12, YMM13, YMM14, YMM15);
		// 2) Declare local variable (cache_indexes, vpmulhuq mask, cache_item_mask).
		asmb.sub(RSP, 0x60); // 3x ymm registers
		// 3) Move data ptr to RBX.
		asmb.mov(RBX, reinterpret_cast<uintptr_t>(asmb.dataPtr()));
		// 4) Prepare const values.
		auto v4q_item_numbers_add = asmb.storeVector4q<uint64_t, RBX>(1, 2, 3, 4); // item_number adder - RBX[0]
		auto v4q_mul_consts = asmb.storeImmediate<uint64_t, Register::YMM(0).size(), RBX>(6364136223846793005ULL); // mul_consts - RBX[32]
		auto v4q_cache_indexes_add = asmb.storeVector4q<uint64_t, RBX>(0, 1, 2, 3); // cache_indexes adder - RBX[64]
		auto v4q_mul_mask = asmb.storeImmediate<uint64_t, Register::YMM(0).size(), RBX>(0xffffffff00000000); // vpmullq mask - RBX[128]
		auto v4q_add_consts = asmb.storeImmediate<uint64_t, Register::YMM(0).size(), RBX>(7009800821677620404ULL); // add_consts - RBX[160]
		auto v4q_item_numbers_step = asmb.storeImmediate<uint64_t, Register::YMM(0).size(), RBX>(4); // item_number adder - RBX[192]

		// 5) Set buffer ptr in R10. [RCX + 0] - pointer in span.
		asmb.mov(R10, RCX[0]);
		// 6) Set buffer size in R13. [RCX + 8] - size of submemory span.
		//    Divide size by 4 (loop counter).
		asmb.mov(R13, RCX[8]);
		asmb.shr(R13, 2);
		// 7) Move cache ptr to YMM6. This register will never be used for anything else.
		asmb.vmovq(XMM6, RDX);
		asmb.vpbroadcastq(YMM6, XMM6);
		auto& cache_ptr_reg = YMM6;

		// Put cache_item_mask on stack.
		asmb.vmovq(XMM3, R08);
		asmb.vpbroadcastq(YMM3, XMM3);
		asmb.vmovdqu(RSP[64], YMM3);
		auto& cache_item_mask = RSP[64];
		// Put vpmulhuq multiplication mask on stack.
		asmb.mov(RAX, 0x00000000ffffffff);
		asmb.vmovq(XMM5, RAX);
		asmb.vpbroadcastq(YMM5, XMM5);
		asmb.vmovdqu(RSP[32], YMM5);
		// 8) Set initial ymmitem0 in YMM7. ymmitem0 = (v4q(start_item) + v4q_item_numbers_add) * v4q_mul_consts
		//    YMM7 will never be used for anything else.
		asmb.vmovq(XMM5, R09);
		asmb.vpbroadcastq(YMM5, XMM5);
		asmb.vpaddq(YMM2, YMM5, v4q_item_numbers_add);
		asmb.vmovdqa(YMM4, v4q_mul_mask);
		asmb.vpmullq(YMM7, YMM2, v4q_mul_consts);
		auto& ymmitem0 = YMM7;
		// 9) Initialize cache indexes. cache_indexes = v4q(start_item) + v4q_cache_indexes_add
		//    This is the only local variable that will be used.
		asmb.vpaddq(YMM2, YMM5, v4q_cache_indexes_add);
		asmb.vmovdqu(RSP[0], YMM2);
		auto& cache_indexes_stack = RSP[0];
		auto& cache_indexes_reg = YMM2;

		// II. Main loop
		// 10) Start loop over all elements.
		asmb.label("loop");

		// 11) Set initial dataset item values.
		// Uses "register-wise" layout:
		// A0 B0 C0 D0
		// A1 B1 C1 D1
		// ...
		// A7 B7 C7 D7
		asmb.vmovdqa(YMM8, YMM7);
		auto xor_imm1 = asmb.storeImmediate<uint64_t, Register::YMM(0).size(), RBX>(9298411001130361340ULL);
		asmb.vpxor(YMM9, YMM7, xor_imm1);
		auto xor_imm2 = asmb.storeImmediate<uint64_t, Register::YMM(0).size(), RBX>(12065312585734608966ULL);
		asmb.vpxor(YMM10, YMM7, xor_imm2);
		auto xor_imm3 = asmb.storeImmediate<uint64_t, Register::YMM(0).size(), RBX>(9306329213124626780ULL);
		asmb.vpxor(YMM11, YMM7, xor_imm3);
		auto xor_imm4 = asmb.storeImmediate<uint64_t, Register::YMM(0).size(), RBX>(5281919268842080866ULL);
		asmb.vpxor(YMM12, YMM7, xor_imm4);
		auto xor_imm5 = asmb.storeImmediate<uint64_t, Register::YMM(0).size(), RBX>(10536153434571861004ULL);
		asmb.vpxor(YMM13, YMM7, xor_imm5);
		auto xor_imm6 = asmb.storeImmediate<uint64_t, Register::YMM(0).size(), RBX>(3398623926847679864ULL);
		asmb.vpxor(YMM14, YMM7, xor_imm6);
		auto xor_imm7 = asmb.storeImmediate<uint64_t, Register::YMM(0).size(), RBX>(9549104520008361294ULL);
		asmb.vpxor(YMM15, YMM7, xor_imm7);

		// 12) Execute all programs.
		for (uint32_t i = 0; i < 8; ++i) {
			const auto& program = programs[i];

			// 16) Prepare registers for program execution.
			asmb.vmovdqa(YMM4, v4q_mul_mask); // Set vpmullq mask in YMM4.
			asmb.vpxor(YMM5, YMM5, YMM5); // vpxor_ret

			// 13) Set cache item indexes.
			if (i == 0) {
				asmb.vpand(YMM0, cache_indexes_reg, YMM3);
			} else {
				// For programs 1-7 address register of the previous program is used.
				const auto cache_indexes_reg_tmp = Register::YMM(8 | programs[i - 1].address_register);
				asmb.vpand(YMM0, cache_indexes_reg_tmp, YMM3);
			}

			// 14) Set cache item pointers.
			static_assert(sizeof(DatasetItem) == 64);
			asmb.vpsllq(YMM0, YMM0, static_cast<int>(std::log2(sizeof(DatasetItem)))); // This will be 6.
			asmb.vpaddq(YMM0, YMM0, cache_ptr_reg);

			// 15) Prefetch cache items.
			asmb.vmovq(RSI, XMM0);
			asmb.prefetchnta(RSI[0]);
			asmb.vpextrq(R14, XMM0, 1);
			asmb.prefetchnta(R14[0]);
			asmb.vextractf128(XMM1, YMM0, 1);
			asmb.vmovq(RDI, XMM1);
			asmb.prefetchnta(RDI[0]);
			asmb.vpextrq(R15, XMM1, 1);
			asmb.prefetchnta(R15[0]);


			// 17) Execute every single instruction of program.
			for (const SuperscalarInstruction& instr : program.instructions) {
				if (instr.type() == SuperscalarInstructionType::INVALID) {
					break;
				}

				emitAVX2Instruction(asmb, instr);
			}

			// 18) Transpose forth and back registers 0-3 and perform XOR with cache items.
			asmb.vpunpcklqdq(YMM0, YMM8, YMM9);			// A0 A1 C0 C1
			asmb.vpunpcklqdq(YMM1, YMM10, YMM11);       // A2 A3 C2 C3
			asmb.vperm2i128(YMM2, YMM0, YMM1, 0x20);	// A0 A1 A2 A3
			asmb.vperm2i128(YMM3, YMM0, YMM1, 0x31);	// C0 C1 C2 C3

			asmb.vpunpckhqdq(YMM0, YMM8, YMM9);         // B0 B1 D0 D1
			asmb.vpunpckhqdq(YMM1, YMM10, YMM11);       // B2 B3 D2 D3
			asmb.vperm2i128(YMM4, YMM0, YMM1, 0x20);    // B0 B1 B2 B3
			asmb.vperm2i128(YMM5, YMM0, YMM1, 0x31);    // D0 D1 D2 D3

			asmb.vpxor(YMM2, YMM2, RSI[0]);
			asmb.vpxor(YMM3, YMM3, RDI[0]);
			asmb.vpxor(YMM4, YMM4, R14[0]);
			asmb.vpxor(YMM5, YMM5, R15[0]);

			// 19) If this is the last program do not transpose registers back and store them in memory.
			if (i == programs.size() - 1) {
				asmb.vmovdqa(R10[0], YMM2);
				asmb.vmovdqa(R10[64], YMM4);
				asmb.vmovdqa(R10[128], YMM3);
				asmb.vmovdqa(R10[192], YMM5);
			} else {
				asmb.vpunpcklqdq(YMM0, YMM2, YMM4);			// A0 B0 A2 B2
				asmb.vpunpcklqdq(YMM1, YMM3, YMM5);			// C0 D0 C2 D2
				asmb.vperm2i128(YMM8, YMM0, YMM1, 0x20);	// A0 B0 C0 D0
				asmb.vperm2i128(YMM10, YMM0, YMM1, 0x31);   // A2 B2 C2 D2

				asmb.vpunpckhqdq(YMM0, YMM2, YMM4);			// A1 B1 A3 B3
				asmb.vpunpckhqdq(YMM1, YMM3, YMM5);			// C1 D1 C3 D3
				asmb.vperm2i128(YMM9, YMM0, YMM1, 0x20);	// A1 B1 C1 D1
				asmb.vperm2i128(YMM11, YMM0, YMM1, 0x31);	// A3 B3 C3 D3
			}

			// 20) Transpose forth and back registers 4-7 and perform XOR with cache items.
			asmb.vpunpcklqdq(YMM0, YMM12, YMM13);		// A4 A5 C4 C5
			asmb.vpunpcklqdq(YMM1, YMM14, YMM15);		// A6 A7 C6 C7
			asmb.vperm2i128(YMM2, YMM0, YMM1, 0x20);	// A4 A5 A6 A7
			asmb.vperm2i128(YMM3, YMM0, YMM1, 0x31);	// C4 C5 C6 C7

			asmb.vpunpckhqdq(YMM0, YMM12, YMM13);		// B4 B5 D4 D5
			asmb.vpunpckhqdq(YMM1, YMM14, YMM15);		// B6 B7 D6 D7
			asmb.vperm2i128(YMM4, YMM0, YMM1, 0x20);	// B4 B5 B6 B7
			asmb.vperm2i128(YMM5, YMM0, YMM1, 0x31);	// D4 D5 D6 D7

			asmb.vpxor(YMM2, YMM2, RSI[32]);
			asmb.vpxor(YMM3, YMM3, RDI[32]);
			asmb.vpxor(YMM4, YMM4, R14[32]);
			asmb.vpxor(YMM5, YMM5, R15[32]);

			// 21) If this is the last program do not transpose registers back and store them in memory.
			if (i == programs.size() - 1) {
				asmb.vmovdqa(R10[32], YMM2);
				asmb.vmovdqa(R10[96], YMM4);
				asmb.vmovdqa(R10[160], YMM3);
				asmb.vmovdqa(R10[224], YMM5);
			} else {
				asmb.vpunpcklqdq(YMM0, YMM2, YMM4);        // A4 B4 A6 B6	
				asmb.vpunpcklqdq(YMM1, YMM3, YMM5);        // C4 D4 C6 D6
				asmb.vperm2i128(YMM12, YMM0, YMM1, 0x20);  // A4 B4 C4 D4
				asmb.vperm2i128(YMM14, YMM0, YMM1, 0x31);  // A6 B6 C6 D6

				asmb.vpunpckhqdq(YMM0, YMM2, YMM4);        // A5 B5 A7 B7
				asmb.vpunpckhqdq(YMM1, YMM3, YMM5);        // C5 D5 C7 D7
				asmb.vperm2i128(YMM13, YMM0, YMM1, 0x20);  // A5 B5 C5 D5
				asmb.vperm2i128(YMM15, YMM0, YMM1, 0x31);  // A7 B7 C7 D7
			}

			// Set cache item mask.
			asmb.vmovdqu(YMM3, cache_item_mask);
		}

		// III. End of loop.
		// 22) Prepare ymmitem0 for next iteration. ymmitem0 = ymmitem0 + v4q_add_consts
		asmb.vpaddq(YMM7, YMM7, v4q_add_consts);
		// 23) Prepare cache_indexes for next iteration. cache_indexes = cache_indexes + v4q_item_numbers_step
		asmb.vmovdqu(cache_indexes_reg, cache_indexes_stack);
		asmb.vpaddq(cache_indexes_reg, cache_indexes_reg, v4q_item_numbers_step);
		asmb.vmovdqu(cache_indexes_stack, cache_indexes_reg);

		// 24) Update dataset pointer. RCX[0] = RCX[0] + 256
		asmb.add(R10, 256);
		// 25) Decrease loop counter.
		asmb.sub(R13, 1);
		asmb.jne("loop");

		// IV. Epilogue
		// 26) Destroy local variable (cache_indexes, vpmulhuq mask, cache_item_mask).
		asmb.add(RSP, 0x60);
		// 27) Pop registers from stack.
		asmb.pop(YMM15, YMM14, YMM13, YMM12, YMM11, YMM10, YMM9, YMM8, YMM7, YMM6, YMM5, YMM4, YMM3, YMM2, YMM1, YMM0, R15, R14, R13, R10, RDI, RSI, RDX, RCX, RBX, RAX);
		// 28) Return.
		asmb.ret();

		// Make the compile code executable and store a pointer to it in the program.
		// Give away ownership of the code and data to the program.
		return makeExecutable<JITDatasetItemProgram, uint8_t>(asmb.flushCode(), asmb.flushData());
	}

	namespace {
		// Translates every single superscalar instruction into native code using AVX2.
		void emitAVX2Instruction(assembler::Context& asmb, const SuperscalarInstruction& instr) noexcept {
			using namespace assembler::registers;
			using namespace assembler;

			const Register dst{ Register::YMM(instr.dst_register | 8) };
			Register src{ Register::YMM(instr.src_register.has_value() ? instr.src_register.value() | 8 : static_cast<reg_idx_t>(0)) };

			switch (instr.type()) {
			case SuperscalarInstructionType::IADD_C7: [[fallthrough]];
			case SuperscalarInstructionType::IADD_C8: [[fallthrough]];
			case SuperscalarInstructionType::IADD_C9:
			{
				auto imm32 = asmb.storeImmediate<int64_t, Register::YMM(0).size(), RBX>(static_cast<int64_t>(static_cast<int32_t>(instr.imm32))); // Store 2's complement of immediate value.
				asmb.vpaddq(dst, dst, imm32);
				break;
			}
			case SuperscalarInstructionType::IXOR_C7: [[fallthrough]];
			case SuperscalarInstructionType::IXOR_C8: [[fallthrough]];
			case SuperscalarInstructionType::IXOR_C9:
			{
				auto imm32 = asmb.storeImmediate<int64_t, Register::YMM(0).size(), RBX>(static_cast<int64_t>(static_cast<int32_t>(instr.imm32))); // Store 2's complement of immediate value.
				asmb.vpxor(dst, dst, imm32);
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
				auto imm64 = asmb.storeImmediate<uint64_t, Register::YMM(0).size(), RBX>(instr.reciprocal);
				asmb.vpmullq(dst, dst, imm64);
				break;
			}
			default:
				std::unreachable();
			}
		}
	}
}
#include "assembler.hpp"
#include "jitcompiler.hpp"
#include "superscalar.hpp"
#include "virtualmem.hpp"


namespace modernRX {
	namespace {
		[[nodiscard]] void emitAVX2Instruction(assembler::Context& asmb, const SuperscalarInstruction& instr) noexcept;
	}

	void compile(SuperscalarProgram& program) {
		using namespace assembler::registers;
		assembler::Context asmb;

		// 0. Prologue - push registers to stack.
		asmb.push(RAX, YMM0, YMM1, YMM2, YMM3, YMM4, YMM5, YMM6, YMM7, YMM8, YMM9, YMM10, YMM11, YMM12, YMM13, YMM14, YMM15);

		// 1. Load every 32-byte value from memory [rcx] into ymm registers.		
		asmb.vmovdqa(YMM8, RCX[0]);
		asmb.vmovdqa(YMM9, RCX[32]);
		asmb.vmovdqa(YMM10, RCX[64]);
		asmb.vmovdqa(YMM11, RCX[96]);
		asmb.vmovdqa(YMM12, RCX[128]);
		asmb.vmovdqa(YMM13, RCX[160]);
		asmb.vmovdqa(YMM14, RCX[192]);
		asmb.vmovdqa(YMM15, RCX[224]);

		// 2. Compile every single instruction.
		for (const SuperscalarInstruction& instr : program.instructions) {
			if (instr.type() == SuperscalarInstructionType::INVALID) {
				break;
			}

			emitAVX2Instruction(asmb, instr);
		}

		// 3. Store every 32-byte value from ymm registers into memory.
		asmb.vmovdqa(RCX[0], YMM8);
		asmb.vmovdqa(RCX[32], YMM9);
		asmb.vmovdqa(RCX[64], YMM10);
		asmb.vmovdqa(RCX[96], YMM11);
		asmb.vmovdqa(RCX[128], YMM12);
		asmb.vmovdqa(RCX[160], YMM13);
		asmb.vmovdqa(RCX[192], YMM14);
		asmb.vmovdqa(RCX[224], YMM15);

		// 4. Epilogue - pop registers and return.
		asmb.pop(YMM15, YMM14, YMM13, YMM12, YMM11, YMM10, YMM9, YMM8, YMM7, YMM6, YMM5, YMM4, YMM3, YMM2, YMM1, YMM0, RAX);
		asmb.ret();

		// Make the compile code executable and store a pointer to it in the program.
		const auto code{ asmb.flush() };
		program.jit_program = makeExecutable<JITSuperscalarProgram, uint8_t>(code);
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
				asmb.movsxd(RAX, instr.imm32);
				asmb.vmovq(XMM0, RAX);
				asmb.vpbroadcastq(YMM0, XMM0);
				asmb.vpaddq(dst, dst, YMM0);
				break;
			case SuperscalarInstructionType::IXOR_C7: [[fallthrough]];
			case SuperscalarInstructionType::IXOR_C8: [[fallthrough]];
			case SuperscalarInstructionType::IXOR_C9:
				asmb.movsxd(RAX, instr.imm32);
				asmb.vmovq(XMM0, RAX);
				asmb.vpbroadcastq(YMM0, XMM0);
				asmb.vpxor(dst, dst, YMM0);
				break;
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
				asmb.vpmullq(dst, src);
				break;
			case SuperscalarInstructionType::ISMULH_R:
				asmb.vpmulhq(dst, src);
				break;
			case SuperscalarInstructionType::IMULH_R:
				asmb.vpmulhuq(dst, dst, src);
				break;
			case SuperscalarInstructionType::IMUL_RCP:
				asmb.mov(RAX, instr.reciprocal);
				asmb.vmovq(XMM0, RAX);
				asmb.vpbroadcastq(YMM7, XMM0);
				asmb.vpmullq(dst, YMM7);
				break;
			default:
				std::unreachable();
			}
		}
	}
}
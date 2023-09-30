﻿#pragma once

/*
* This file contains functions for generating binary code for x86_64 architecture.
* It is not a complete assembler, only instructions needed for JIT Compiler used for RandomX programs.
* Code may be a little bit messy and not fully documented as this will be further extended in unknown direction.
*/

#include <array>
#include <iterator>
#include <vector>


namespace modernRX::assembler {
	using reg_idx_t = uint8_t;

	enum class RegisterType : uint8_t {
		GPR = 0, XMM = 1, YMM = 2, ZMM = 4
	};

	enum class PP : uint8_t {
		PP0x00 = 0, PP0x66 = 1, PP0xF3 = 2, PP0xF2 = 3
	};

	enum class MOD : uint8_t {
		MOD00 = 0, MOD01 = 1, MOD10 = 2, MOD11 = 3
	};

	enum class MM : uint8_t {
		MM0x0F = 1, MM0x0F38 = 2, MM0x0F3A = 3
	};

	enum class SCALE : uint8_t {
		SS1 = 0, SS2 = 1, SS4 = 2, SS8 = 3
	};

	struct Opcode {
		uint8_t code;
		int8_t mod{ -1 }; // -1 - no value; cannot be std::optional because of language limitations.
	};

	struct Memory {
		reg_idx_t reg;
		int32_t offset;

		[[nodiscard]] constexpr bool isLow() const noexcept {
			return reg < 8;
		}

		[[nodiscard]] constexpr bool operator==(const Memory& rhs) const noexcept {
			return reg == rhs.reg && offset == rhs.offset;
		}
	};

	struct Register {
		RegisterType type;
		reg_idx_t idx;

		[[nodiscard]] static constexpr Register YMM(const reg_idx_t idx) noexcept {
			return Register{ RegisterType::YMM, idx };
		}

		[[nodiscard]] constexpr bool isLow() const noexcept {
			return idx < 8;
		}

		[[nodiscard]] constexpr bool isHigh() const noexcept {
			return idx >= 8;
		}

		[[nodiscard]] constexpr reg_idx_t lowIdx() const noexcept {
            return idx % 8;
        }

		[[nodiscard]] constexpr int32_t size() const noexcept {
			if (type == RegisterType::GPR) {
				return 8;
			} else if (type == RegisterType::XMM) {
				return 16;
			} else if (type == RegisterType::YMM) {
				return 32;
			} else if (type == RegisterType::ZMM) {
				return 64;
			}

			std::unreachable();
		}

		[[nodiscard]] constexpr bool operator==(const Register& rhs) const noexcept {
			return type == rhs.type && idx == rhs.idx;
		}

		[[nodiscard]] constexpr const Memory operator[](const int32_t offset) const noexcept {
            return Memory{ idx, offset };
        }
	};

	namespace registers {
		inline constexpr Register RAX{ RegisterType::GPR, 0 };
		inline constexpr Register RCX{ RegisterType::GPR, 1 };
		inline constexpr Register RDX{ RegisterType::GPR, 2 };
		inline constexpr Register RBX{ RegisterType::GPR, 3 };
		inline constexpr Register RSP{ RegisterType::GPR, 4 };
		inline constexpr Register RBP{ RegisterType::GPR, 5 };
		const Register& RIP{ RBP }; // RIP is an alias for RBP.
		inline constexpr Register RSI{ RegisterType::GPR, 6 };
		inline constexpr Register RDI{ RegisterType::GPR, 7 };
		inline constexpr Register R08{ RegisterType::GPR, 8 };
		inline constexpr Register R09{ RegisterType::GPR, 9 };
		inline constexpr Register R10{ RegisterType::GPR, 10 };
		inline constexpr Register R11{ RegisterType::GPR, 11 };
		inline constexpr Register R12{ RegisterType::GPR, 12 };
		inline constexpr Register R13{ RegisterType::GPR, 13 };
		inline constexpr Register R14{ RegisterType::GPR, 14 };
		inline constexpr Register R15{ RegisterType::GPR, 15 };

		inline constexpr Register XMM0{ RegisterType::XMM, 0 };
		inline constexpr Register XMM1{ RegisterType::XMM, 1 };
		inline constexpr Register XMM2{ RegisterType::XMM, 2 };
		inline constexpr Register XMM3{ RegisterType::XMM, 3 };
		inline constexpr Register XMM4{ RegisterType::XMM, 4 };
		inline constexpr Register XMM5{ RegisterType::XMM, 5 };
		inline constexpr Register XMM6{ RegisterType::XMM, 6 };
		inline constexpr Register XMM7{ RegisterType::XMM, 7 };
		inline constexpr Register XMM8{ RegisterType::XMM, 8 };
		inline constexpr Register XMM9{ RegisterType::XMM, 9 };
		inline constexpr Register XMM10{ RegisterType::XMM, 10 };
		inline constexpr Register XMM11{ RegisterType::XMM, 11 };
		inline constexpr Register XMM12{ RegisterType::XMM, 12 };
		inline constexpr Register XMM13{ RegisterType::XMM, 13 };
		inline constexpr Register XMM14{ RegisterType::XMM, 14 };
		inline constexpr Register XMM15{ RegisterType::XMM, 15 };

		inline constexpr Register YMM0{ RegisterType::YMM, 0 };
		inline constexpr Register YMM1{ RegisterType::YMM, 1 };
		inline constexpr Register YMM2{ RegisterType::YMM, 2 };
		inline constexpr Register YMM3{ RegisterType::YMM, 3 };
		inline constexpr Register YMM4{ RegisterType::YMM, 4 };
		inline constexpr Register YMM5{ RegisterType::YMM, 5 };
		inline constexpr Register YMM6{ RegisterType::YMM, 6 };
		inline constexpr Register YMM7{ RegisterType::YMM, 7 };
		inline constexpr Register YMM8{ RegisterType::YMM, 8 };
		inline constexpr Register YMM9{ RegisterType::YMM, 9 };
		inline constexpr Register YMM10{ RegisterType::YMM, 10 };
		inline constexpr Register YMM11{ RegisterType::YMM, 11 };
		inline constexpr Register YMM12{ RegisterType::YMM, 12 };
		inline constexpr Register YMM13{ RegisterType::YMM, 13 };
		inline constexpr Register YMM14{ RegisterType::YMM, 14 };
		inline constexpr Register YMM15{ RegisterType::YMM, 15 };
	}


	// generates 3rd byte of VEX prefix.
	// Len = 1 - 256-bit instruction.
	// Len = 0 - 128-bit instruction.
	// In case when using 2-byte VEX prefix, this function may be used to generate 2nd byte.
	// In such case, W is always ignored and the w param is in fact an R.
	template<typename Ret = std::byte>
	[[nodiscard]] constexpr Ret vex3(const reg_idx_t src1, const PP pp, const int w = 0, const uint8_t len = 1) noexcept {
		// bit		7	6	5	4	3	2	1	0
		//			W/R	v̅3 v̅2 v̅1 v̅0 L	p1	p0
		uint8_t src_val = ~src1; // 1's complement; 15 -> ymm0, 14 -> ymm1 ... 0 -> ymm15
		src_val &= 0x0f; // zero out upper bits
		return static_cast<Ret>((w << 7) | (src_val << 3) | (len << 2) | static_cast<uint8_t>(pp));
	}

	// generates 2rd byte of VEX prefix.
	template<typename Ret = std::byte>
	[[nodiscard]] constexpr Ret vex2(const MM mm, const int r = 0, const int x = 0, const int b = 0) noexcept {
		// bit		7	6	5	4	3	2	1	0
		//			R̅ 	X̅ 	B̅ 	m4 	m3 	m2 	m1 	m0

		return static_cast<Ret>((r << 7) | (x << 6) | (b << 5) | static_cast<uint8_t>(mm));
	}

	// generates 2rd byte of VEX prefix.
	template<typename Ret = std::byte>
	[[nodiscard]] constexpr Ret vex2(const reg_idx_t src1, const PP pp, const int r = 0, const uint8_t len = 1) noexcept {
		// bit		7	6	5	4	3	2	1	0
		//			R	v̅3 v̅2 v̅1 v̅0 L	p1	p0
		uint8_t src_val = ~src1; // 1's complement; 15 -> ymm0, 14 -> ymm1 ... 0 -> ymm15
		src_val &= 0x0f; // zero out upper bits
		return static_cast<Ret>((r << 7) | (src_val << 3) | (len << 2) | static_cast<uint8_t>(pp));
	}

	// generates 1rd byte of VEX prefix.
	template<typename Ret = std::byte>
	[[nodiscard]] constexpr Ret vex1(const bool three_byte = true) noexcept {
		return static_cast<Ret>(three_byte ? 0xc4 : 0xc5);
	}

	// by default uses MOD11 (register addressing)
	template<typename Ret = std::byte>
	[[nodiscard]] constexpr Ret modregrm(const reg_idx_t reg, const reg_idx_t rm, const MOD mod = MOD::MOD11) noexcept {
		// MOD = 11 (register addressing), REG = dst_reg, R/M = src_reg
		return static_cast<Ret>((static_cast<uint8_t>(mod) << 6) | (reg << 3) | rm);
	}



	template<typename Ret = std::byte>
	[[nodiscard]] constexpr Ret rex(const int w = 0, const int r = 0, const int x = 0, const int b = 0) noexcept {
		// bit		7	6	5	4	3	2	1	0
		//			0   1   0   0   W   R   X   B
		return static_cast<Ret>((0b0100 << 4) | (w << 3) | (r << 2) | (x << 1) | b);
	}


	// Index cannot be 4.
	template <typename Ret = std::byte>
	[[nodiscard]] constexpr Ret sib(const SCALE scale, const Register index, const Register base) noexcept {

		// bit		7	6	5	4	3	2	1	0
		//			S   S  	I   I   I   B   B   B  ; S - scale; I - index; B - base
		return static_cast<Ret>((static_cast<uint8_t>(scale) << 6) | (index.lowIdx() << 3) | base.lowIdx());
	}


	template<int Byte, typename Val>
	[[nodiscard]] constexpr std::byte byte(const Val& v) noexcept {
		static_assert(Byte < sizeof(Val));
		return *(reinterpret_cast<const std::byte*>(&v) + Byte);
	}

	class Context {
	public: 
		explicit Context() {
			code.reserve(4096);
			data.reserve(4096);
		}

		// Stores an immediate value in the data buffer.
		template<typename Immediate, size_t Bytes>
			requires (std::is_integral_v<Immediate>&& Bytes % sizeof(Immediate) == 0)
		constexpr void storeImmediate(const Immediate imm) {
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
		}

		template<typename Operand>
		constexpr void vpmuludq(const Register dst_reg, const Register src_reg1, const Operand src_reg2) {
			vex256<PP::PP0x66, MM::MM0x0F, Opcode{ 0xf4, -1 }>(dst_reg, src_reg1, src_reg2);
		}
		
		template<typename Operand>
		constexpr void vpshufd(const Register dst_reg, const Register src_reg, const Operand control) {
			vex256<PP::PP0x66, MM::MM0x0F, Opcode{ 0x70, -1 }>(dst_reg, src_reg, control);
		}

		// https://stackoverflow.com/a/28827013
		// Multiply packed unsigned quadwords and store high result.
		// This is emulated instruction (not available in AVX2).
		// Requires an 0xffffffff mask in YMM7 register.
		// Uses YMM1-YMM6 registers.
		constexpr void vpmulhuq(const Register dst_reg, const Register src_reg1, const Register src_reg2) {
			vpshufd(registers::YMM1, src_reg1, 0xb1); // vpshufd dst
			vpshufd(registers::YMM2, src_reg2, 0xb1); // vpshufd src
			vpmuludq(registers::YMM3, src_reg1, src_reg2); // vpmuludq_w0
			vpsrlq(registers::YMM3, registers::YMM3, 32); // vpsrlq_w0h
			vpmuludq(registers::YMM4, src_reg1, registers::YMM2); // vpmuludq_w1
			vpaddq(registers::YMM3, registers::YMM3, registers::YMM4); // vpaddq_s1)
			vpand(registers::YMM4, registers::YMM3, registers::YMM7); // vpand_s1l
			vpsrlq(registers::YMM3, registers::YMM3, 32); // vpsrlq_s1h
			vpmuludq(registers::YMM5, registers::YMM1, src_reg2); // vpmuludq_w2
			vpaddq(registers::YMM4, registers::YMM4, registers::YMM5); // vpaddq_s2
		    vpsrlq(registers::YMM4, registers::YMM4, 32); // vpsrlq_s2h
			vpmuludq(registers::YMM0, registers::YMM1, registers::YMM2); // vpmuludq_w3
			vpaddq(registers::YMM0, registers::YMM0, registers::YMM3); // vpaddq_hi
			vpaddq(dst_reg, registers::YMM0, registers::YMM4); // vpaddq_ret)
		}



		// https://stackoverflow.com/a/28827013
		// Multiply packed quadwords and store high result.
		// This is emulated instruction (not available in AVX2).
		// Uses YMM0, YMM1, YMM5 and YMM6 registers.
		constexpr void vpmulhq(const Register dst_reg, const Register src_reg) {
			vpmulhuq(registers::YMM5, dst_reg, src_reg);
			vpcmpgtq(registers::YMM0, registers::YMM6, dst_reg);
			vpcmpgtq(registers::YMM1, registers::YMM6, src_reg);
			vpand(registers::YMM0, registers::YMM0, src_reg);
			vpand(registers::YMM1, registers::YMM1, dst_reg);
			vpsubq(registers::YMM5, registers::YMM5, registers::YMM0);
			vpsubq(dst_reg, registers::YMM5, registers::YMM1);
		}

		// https://stackoverflow.com/a/37322570
		// Multiply packed quadwords and store low result.
		// This is emulated instruction (not available in AVX2).
		// Requires YMM6 to be zeroed.
		// Uses YMM0-YMM3 registers.
		constexpr void vpmullq(const Register dst_reg, const Register src_reg) {
			vpshufd(registers::YMM0, src_reg, 0xb1);
			// VEX.256.66.0F38.WIG 40 /r VPMULLD ymm1, ymm2, ymm3/m256
			vex256 < PP::PP0x66, MM::MM0x0F38, Opcode{ 0x40, -1 } > (registers::YMM1, dst_reg, registers::YMM0);
			// VEX.256.66.0F38.WIG 02 /r VPHADDD ymm1, ymm2, ymm3/m256
			vex256 < PP::PP0x66, MM::MM0x0F38, Opcode{ 0x02, -1 } > (registers::YMM2, registers::YMM1, registers::YMM6);
			vpshufd(registers::YMM2, registers::YMM2, 0x73);
			vpmuludq(registers::YMM3, dst_reg, src_reg);
			vpaddq(dst_reg, registers::YMM3, registers::YMM2);
		}
		
		constexpr void vpbroadcastq(const Register ymm, const Register xmm) {
			vex256< PP::PP0x66, MM::MM0x0F38, Opcode{ 0x59, -1 }>(ymm, xmm);
		}

		// Moves 64-bit value from GPR to XMM register.
		constexpr void vmovq(const Register dst, const Register src) {
			// vmovq xmm, gpr: {vex.3B: 0xc4} {vex.0bR0B'00001} {vex.0b1'....'0'66} {opcode: #L:0x6e|#S:0x7e /r} {modrm: 0b11'xmm'gpr}
			code.push_back(vex1<uint8_t>(true));
			code.push_back(vex2<uint8_t>(MM::MM0x0F, dst.isLow(), 0, src.isLow()));
			code.push_back(vex3<uint8_t>(0, PP::PP0x66, 1, 0));
			code.push_back(dst.type == RegisterType::XMM ? 0x6e : 0x7e);
			code.push_back(modregrm<uint8_t>(dst.lowIdx(), src.lowIdx()));
		}

		// Moves 32 or 64-bit immediate value into register.
		constexpr void mov(const Register gpr, const uint64_t imm64) {
			// mov gpr, imm32/64: [rex.w] {opcode: 0xb8+ rd io} {imm64}
			if (imm64 > std::numeric_limits<uint32_t>::max()) {
				code.push_back(rex<uint8_t>(1, gpr.isHigh()));
			}

			code.push_back(0xb8 + gpr.lowIdx());
			code.push_back((uint8_t)byte<0>(imm64));
			code.push_back((uint8_t)byte<1>(imm64));
			code.push_back((uint8_t)byte<2>(imm64));
			code.push_back((uint8_t)byte<3>(imm64));

			if (imm64 > std::numeric_limits<uint32_t>::max()) {
				code.push_back((uint8_t)byte<4>(imm64));
				code.push_back((uint8_t)byte<5>(imm64));
				code.push_back((uint8_t)byte<6>(imm64));
				code.push_back((uint8_t)byte<7>(imm64));
			}
		}

		// It uses mov instruction to move imm32 to gpr register, because movsxd cannot be used with immediate value.
		constexpr void movsxd(const Register gpr, const int32_t imm32) {
			// mov gpr, imm32: {rex.w} {opcode: 0xc7 /0} {modrm: 0b11'000'gpr} {imm32}
			rexw<Opcode{ 0xc7, 0 }>(gpr, imm32);
		}


		// Dst and Src must be either Register or Memory, but not both.
		template<typename Dst, typename Src>
		requires (
			(std::is_same_v<Dst, Register> || std::is_same_v<Dst, Memory>) && 
			(std::is_same_v<Src, Register> || std::is_same_v<Src, Memory>) &&
			(!std::is_same_v<Dst, Src> || (std::is_same_v<Dst, Register> && std::is_same_v<Src, Register>))
		)
		constexpr void vmovdqa(const Dst dst, const Src src) {
			static_assert(!std::is_same_v<Dst, Src>, "register mov not implemented");

			// vmovqdu ymmX, ymmword ptr [gpr + offset]: {vex.2B: 0xc5} {vex.0bR'0000'1'66} {opcode: #L:0x6f|#S:0x7f /r} {modrm: 0b00'ymm'gpr} [sib: rsp] [disp8/32]
			if constexpr (std::is_same_v<Dst, Register>) {
				vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0x6f, -1 } > (dst, src);
			} else {
				vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0x7f, -1 } > (src, dst);
			}
		}

		// Dst and Src must be either Register or Memory, but not both.
		template<typename Dst, typename Src>
		requires (
			(std::is_same_v<Dst, Register> || std::is_same_v<Dst, Memory>) &&
			(std::is_same_v<Src, Register> || std::is_same_v<Src, Memory>) &&
			(!std::is_same_v<Dst, Src> || (std::is_same_v<Dst, Register> && std::is_same_v<Src, Register>))
		)
		constexpr void vmovdqu(const Dst dst, const Src src) {
			static_assert(!std::is_same_v<Dst, Src>, "register mov not implemented");

			// vmovqdu ymmX, ymmword ptr [gpr + offset]: {vex.2B: 0xc5} {vex.0bR'0000'1'66} {opcode: #L:0x6f|#S:0x7f /r} {modrm: 0b00'ymm'gpr} [sib: rsp] [disp8/32]
			if constexpr (std::is_same_v<Dst, Register>) {
				vex256< PP::PP0xF3, MM::MM0x0F, Opcode{ 0x6f, -1 }>(dst, src);
			} else {
				vex256< PP::PP0xF3, MM::MM0x0F, Opcode{ 0x7f, -1 }>(src, dst);
			}
		}

		template<typename Operand>
		constexpr void vpsrlq(const Register dst_reg, const Register src_reg, const Operand shift) noexcept {
			vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0x73, 2 } > (dst_reg, src_reg, shift);
		}

		template<typename Operand>
		constexpr void vpsllq(const Register dst_reg, const Register src_reg, const Operand shift) noexcept {
			vex256< PP::PP0x66, MM::MM0x0F, Opcode{ 0x73, 6 }>(dst_reg, src_reg, shift);
		}


		template<typename Operand>
		constexpr void vpsubq(const Register dst_reg, const Register src_reg1, const Operand src_reg2) noexcept {
			vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0xfb, -1 } > (dst_reg, src_reg1, src_reg2);
		}

		template<typename Operand>
		constexpr void vpcmpgtq(const Register dst_reg, const Register src_reg1, const Operand src_reg2) noexcept {
			vex256< PP::PP0x66, MM::MM0x0F38, Opcode{ 0x37, -1 }>(dst_reg, src_reg1, src_reg2);
		}

		template<typename Operand>
		constexpr void vpaddq(const Register dst_reg, const Register src_reg1, const Operand src_reg2) noexcept {
			vex256< PP::PP0x66, MM::MM0x0F, Opcode{ 0xd4, -1 }>(dst_reg, src_reg1, src_reg2);
		}

		template<typename Operand>
		constexpr void vpxor(const Register dst_reg, const Register src_reg1, const Operand src_reg2) noexcept {
			vex256< PP::PP0x66, MM::MM0x0F, Opcode{ 0xef, -1 }>(dst_reg, src_reg1, src_reg2);
		}

		template<typename Operand>
		constexpr void vpand(const Register dst_reg, const Register src_reg1, const Operand src_reg2) noexcept {
			vex256< PP::PP0x66, MM::MM0x0F, Opcode{ 0xdb, -1 }>(dst_reg, src_reg1, src_reg2);
		}

		template<typename Operand>
		constexpr void vpor(const Register dst_reg, const Register src_reg1, const Operand src_reg2) noexcept {
			vex256 < PP::PP0x66, MM::MM0x0F, Opcode{ 0xeb, -1 } > (dst_reg, src_reg1, src_reg2);
		}

		// Pushes single GPR register on stack.
		constexpr void push(const Register reg) {
            if (reg.isHigh()) {
                code.push_back(rex<uint8_t>(0, 0, 0, 1));
            }
            code.push_back(0x50 + reg.lowIdx());
		}

		// Pops single GPR register from stack.
		constexpr void pop(const Register reg) {
			if (reg.isHigh()) {
				code.push_back(rex<uint8_t>(0, 0, 0, 1));
			}
			code.push_back(0x58 + reg.lowIdx());
		}

		constexpr void sub(const Register dst, const int32_t size) {
			if (size < std::numeric_limits<int8_t>::max()) {
				rexw < Opcode{ 0x83, 5 } > (dst, static_cast<int8_t>(size));
			} else {
                rexw < Opcode{ 0x81, 5 } > (dst, size);
            }
		}

		constexpr void add(const Register dst, const int32_t size) {
			if (size < std::numeric_limits<int8_t>::max()) {
				rexw < Opcode{ 0x83, 0 } > (dst, static_cast<int8_t>(size));
			} else {
				rexw < Opcode{ 0x81, 0 } > (dst, size);
			}
		}

		// Generates binary code for pushing registers on stack.
		// For general purpose registers it uses PUSH instruction.
		// For YMM registers it expands stack and copies register to stack.
		// Example:
		// context.push(RAX, R8, YMM1, YMM0) will generate:
		//
		// push rax
		// push r8
		// sub rsp, 0x40
		// vmovqdu ymmword ptr [rsp], ymm1
		// vmovqdu ymmword ptr [rsp + 0x20], ymm0
		template<typename... Reg>
		requires (std::is_same_v<Reg, Register> && ...)
		constexpr void push(Reg... regs) {
			static_assert(sizeof...(regs) > 0);

			std::array<Register, sizeof...(regs)> ymm_regs{};
			int32_t ymm_regs_cnt{ 0 };

			for (const auto& reg : { regs... }) {
				// General purpose registers are pushed on stack using PUSH instruction: 
				if (reg.type == RegisterType::GPR) {
					push(reg);
				}
				// YMM registers are pushed on stack by expanding stack and copying register to stack.
				// Lets just count how many YMM registers was passed and push them at the end, because they may be between general purpose registers.
				else if (reg.type == RegisterType::YMM) {
					ymm_regs[ymm_regs_cnt++] = reg;
				}
            }

			if (ymm_regs_cnt > 0) {
				const int32_t stack_size{ ymm_regs_cnt * Register::YMM(0).size() };
				sub(registers::RSP, stack_size);

				for (int32_t i = ymm_regs_cnt - 1; i >= 0; --i) {
					const auto& reg{ ymm_regs[i] };
                    const int32_t stack_offset{ (ymm_regs_cnt - 1 - i) * reg.size() };

					vmovdqu(registers::RSP[stack_offset], reg);
				}
			}
		}

		// Generates binary code for poping registers from stack.
		// For general purpose registers it uses POP instruction.
		// For YMM registers it copies values from stack to registers.
		// It is important to use this function with registers in reverse order of adequate push function.
		// Example:
		// context.pop(YMM0, YMM1, R8, RAX) will generate:
		//
		// vmovqdu ymm0, ymmword ptr [rsp + 0x20]
		// vmovqdu ymm1, ymmword ptr [rsp]
		// add rsp, 0x40
		// pop r8
		// pop rax
		template<typename... Reg>
		requires (std::is_same_v<Reg, Register> && ...)
		constexpr void pop(Reg... regs) {
			static_assert(sizeof...(regs) > 0);

			std::array<Register, sizeof...(regs)> ymm_regs{};
			int32_t ymm_regs_cnt{ 0 };

			for (const auto& reg : { regs... }) {
				// YMM registers are popped from stack by copying values from stack back to register and shrinking stack.
				// Lets just count how many YMM registers was passed and pop them right after, when stack size reserved for them is known.
				if (reg.type == RegisterType::YMM) {
					ymm_regs[ymm_regs_cnt++] = reg;
				}
			}

			if (ymm_regs_cnt > 0) {
				for (int32_t i = 0; i < ymm_regs_cnt; ++i) {
					const auto& reg{ ymm_regs[i] };
					const int32_t stack_offset{ i * reg.size() };

					// vmovqdu ymmX, ymmword ptr [rsp + stack_offset]: {vex.2B: 0xc5} {vex.0bR'0000'1'F3} {opcode: 0x6f /r} {modrm: 0b00'reg.idx'100} {sib: rsp + disp8/32} {disp8/32}
					vmovdqu(reg, registers::RSP[stack_offset]);
				}

				const int32_t stack_size{ ymm_regs_cnt * Register::YMM(0).size() };
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
			for (int s = size; s > 0; ) {
				switch (s % 8) {
					case 1: code.push_back(0x90); --s; break;
					case 2: code.append_range(std::vector<uint8_t>{ 0x66, 0x90 }); s -= 2; break;
					case 3: code.append_range(std::vector<uint8_t>{ 0x0f, 0x1f, 0x00 }); s -= 3; break;
					case 4: code.append_range(std::vector<uint8_t>{ 0x0f, 0x1f, 0x40, 0x00 }); s -= 4; break;
					case 5: code.append_range(std::vector<uint8_t>{ 0x0f, 0x1f, 0x44, 0x00, 0x00 }); s -= 5; break;
					case 6: code.append_range(std::vector<uint8_t>{ 0x66, 0x0f, 0x1f, 0x44, 0x00, 0x00 }); s -= 6; break;
					case 7: code.append_range(std::vector<uint8_t>{ 0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00 }); s -= 7; break;
				}
			}
		}

		// Generates ret instruction if previous byte isnt ret opcode already.
		constexpr void ret() {
			if (code.back() == 0xc3) {
				return;
			}

			code.push_back(0xc3);
		}

		// Flushes generated code that results in giving away ownership of the buffer.
		[[nodiscard]] std::vector<uint8_t> flushCode() {
			return std::move(code);
		}

		// Flushes generated data that results in giving away ownership of the buffer.
		[[nodiscard]] std::vector<uint8_t> flushData() {
			return std::move(data);
		}

		[[nodiscard]] constexpr const uint8_t* dataPtr() noexcept {
			return data.data();
		}

		[[nodiscard]] constexpr const int32_t dataSize() noexcept {
			return static_cast<int32_t>(data.size());
		}

	private:
		std::vector<uint8_t> code;
		std::vector<uint8_t> data;

		// Generates bytes for indirect addressing mode. 
		constexpr void addr(const reg_idx_t dst, const reg_idx_t src, const int32_t offset) {
			// RIP relative addressing (displacement only).
			if (src == registers::RBP.idx) {
				code.push_back(modregrm<uint8_t>(0, registers::RBP.idx, MOD::MOD00)); // Indirect or SIB mode if RSP used.
				code.push_back((uint8_t)byte<0>(offset));
				code.push_back((uint8_t)byte<1>(offset));
				code.push_back((uint8_t)byte<2>(offset));
				code.push_back((uint8_t)byte<3>(offset));
				return;
			}

			if (offset == 0) {
				code.push_back(modregrm<uint8_t>(dst % 8, src, MOD::MOD00)); // Indirect or SIB mode if RSP used.
			} else if (offset < std::numeric_limits<int8_t>::max()) {
				code.push_back(modregrm<uint8_t>(dst % 8, src, MOD::MOD01)); // Indirect + disp8 or SIB mode + disp8 if RSP used.
			} else {
				code.push_back(modregrm<uint8_t>(dst % 8, src, MOD::MOD10)); // Indirect + disp32 or SIB mode + disp32 if RSP used.
			}

			// SIB byte.
			if (src == registers::RSP.idx) {
				code.push_back(sib<uint8_t>(SCALE::SS1, registers::RSP, registers::RSP));
			}

			// Disp8
			if (offset > 0) {
				code.push_back((uint8_t)byte<0>(offset));
			}

			// Disp32
			if (offset >= std::numeric_limits<int8_t>::max()) {
				code.push_back((uint8_t)byte<1>(offset));
				code.push_back((uint8_t)byte<2>(offset));
				code.push_back((uint8_t)byte<3>(offset));
			}
		}

		// Generates instruction with REX.W prefix and Register/IMM as operands.
		template<Opcode opcode, typename Imm>
		requires (std::is_same_v<Imm, int8_t> || std::is_same_v<Imm, int32_t> || std::is_same_v<Imm, int64_t>)
		constexpr void rexw(const Register dst, const Imm imm) {
			code.push_back(rex<uint8_t>(1, 0, 0, dst.isHigh()));
			code.push_back(opcode.code);
			code.push_back(modregrm<uint8_t>(std::max<reg_idx_t>(opcode.mod, 0), dst.lowIdx()));

			code.push_back((uint8_t)byte<0>(imm));

			if constexpr (std::is_same_v<Imm, int32_t>) {
				code.push_back((uint8_t)byte<1>(imm));
				code.push_back((uint8_t)byte<2>(imm));
				code.push_back((uint8_t)byte<3>(imm));
			}

			if constexpr (std::is_same_v<Imm, int64_t>) {
				code.push_back((uint8_t)byte<4>(imm));
				code.push_back((uint8_t)byte<5>(imm));
				code.push_back((uint8_t)byte<6>(imm));
				code.push_back((uint8_t)byte<7>(imm));
			}
		}

		// Generates instruction with VEX256 prefix and Register/Register as operands (eg. vpbroadcastq).
		template<PP pp, MM mm, Opcode opcode, uint8_t w = 0>
		constexpr void vex256(const Register dst, const Register src1) {
			if constexpr (mm == MM::MM0x0F) {
				if (dst.isLow()) {
					// INSTR ymmD, ymmS1, ymmS2: {vex.2B: 0xc5} {vex.0b1'ffff'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
					code.push_back(vex1<uint8_t>(false));
					code.push_back(vex2<uint8_t>(0, pp, 1));
				} else {
					// INSTR ymmD, ymmS1, ymmS2: {vex.3B: 0xc4} {vex.0bR0B'00001} {vex.0bW'ffff'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
					code.push_back(vex1<uint8_t>(true));
					code.push_back(vex2<uint8_t>(MM::MM0x0F, dst.isLow(), 0, src1.isLow()));
					code.push_back(vex3<uint8_t>(0, pp, w));
				}
			} else {
				code.push_back(vex1<uint8_t>(true));
				code.push_back(vex2<uint8_t>(mm, dst.isLow(), 0, src1.isLow()));
				code.push_back(vex3<uint8_t>(0, pp, w));
			}

			code.push_back(opcode.code);
			code.push_back(modregrm<uint8_t>(dst.lowIdx(), src1.lowIdx()));
		}

		// Generates instruction with VEX256 prefix and Register/Memory as operands (eg. vmovdqa).
		template<PP pp, MM mm, Opcode opcode, uint8_t w = 0>
		constexpr void vex256(const Register dst, const Memory src1) {
			if constexpr (mm == MM::MM0x0F) {
				if (src1.isLow()) {
					// INSTR ymmD, ymmS1, ymmS2: {vex.2B: 0xc5} {vex.0b1'ffff'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
					code.push_back(vex1<uint8_t>(false));
					code.push_back(vex2<uint8_t>(0, pp, dst.isLow()));
				} else {
					// INSTR ymmD, ymmS1, ymmS2: {vex.3B: 0xc4} {vex.0bR0B'00001} {vex.0bW'ffff'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
					code.push_back(vex1<uint8_t>(true));
					code.push_back(vex2<uint8_t>(MM::MM0x0F, dst.isLow(), 0, src1.isLow()));
					code.push_back(vex3<uint8_t>(0, pp, w));
				}
			} else {
				code.push_back(vex1<uint8_t>(true));
				code.push_back(vex2<uint8_t>(mm, dst.isLow(), 0, src1.isLow()));
				code.push_back(vex3<uint8_t>(0, pp, w));
			}
		
			code.push_back(opcode.code);
			addr(dst.idx, src1.reg, src1.offset);
		}

		// Generates instruction with VEX256 prefix and Register/Register/Register as operands (eg. vpxor).
		template<PP pp, MM mm, Opcode opcode, uint8_t w = 0>
		constexpr void vex256(const Register dst, const Register src1, const Register src2) {
			if constexpr (mm == MM::MM0x0F) {
				if (src2.isLow() && dst.isLow()) {
					// INSTR ymmD, ymmS1, ymmS2: {vex.2B: 0xc5} {vex.0b1'sss!'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
					code.push_back(vex1<uint8_t>(false));
					code.push_back(vex2<uint8_t>(src1.idx, pp, 1));
				} else {
					// INSTR ymmD, ymmS1, ymmS2: {vex.3B: 0xc4} {vex.0bR0B'00001} {vex.0bW'sss!'1'66} {opcode.code /opcode.mod} {modrm: 0b11'ddd'ss@}
					code.push_back(vex1<uint8_t>(true));
					code.push_back(vex2<uint8_t>(MM::MM0x0F, dst.isLow(), 0, src2.isLow()));
					code.push_back(vex3<uint8_t>(src1.idx, pp, w));
				}
			} else {
				code.push_back(vex1<uint8_t>(true));
				code.push_back(vex2<uint8_t>(mm, dst.isLow(), 0, src2.isLow()));
				code.push_back(vex3<uint8_t>(src1.idx, pp, w));
			}

			code.push_back(opcode.code);
			code.push_back(modregrm<uint8_t>(dst.lowIdx(), src2.lowIdx()));
		}

		// Generates instruction with VEX256 prefix and Register/Register/Imm as operands (eg. vpsllq).
		template<PP pp, MM mm, Opcode opcode, uint8_t w = 0> 
		constexpr void vex256(const Register dst, const Register src1, const int imm32) {
			const reg_idx_t vvvv{ opcode.mod > -1 ? dst.idx : uint8_t(0) };
			const reg_idx_t reg{ opcode.mod > -1 ? (uint8_t)opcode.mod : dst.lowIdx() };

			if constexpr (mm == MM::MM0x0F) {
				if (src1.isLow()) {
					// INSTR ymmD, ymmS1, imm8: {vex.2B: 0xc5} {vex.0b1'dddd'1'66} {opcode.code /opcode.mod} {modrm: 0b11'mod'ss!}
					code.push_back(vex1<uint8_t>(false));
					code.push_back(vex2<uint8_t>(vvvv, pp, 1));
				} else {
					// INSTR ymmD, ymmS1, imm8: {vex.3B: 0xc4} {vex.0b000'00001} {vex.0bW'dddd'1'66} {opcode.code /opcode.mod} {modrm: 0b11'mod'ss!}
					code.push_back(vex1<uint8_t>(true));
					code.push_back(vex2<uint8_t>(MM::MM0x0F, dst.isLow(), 0, src1.isLow()));
					code.push_back(vex3<uint8_t>(vvvv, pp, w));
				}
			} else {
				code.push_back(vex1<uint8_t>(true));
				code.push_back(vex2<uint8_t>(mm, dst.isLow(), 0, src1.isLow()));
				code.push_back(vex3<uint8_t>(vvvv, pp, w));
			}

			code.push_back(opcode.code);
			code.push_back(modregrm<uint8_t>(reg, src1.lowIdx()));
			code.push_back((uint8_t)byte<0>(imm32));
		}
	};
}
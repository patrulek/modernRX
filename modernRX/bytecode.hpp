#pragma once

#include <cstdint>
#include <array>

namespace modernRX {

	// Defines all instructions in RandomX programs.
	enum class Bytecode : uint8_t {
		ISUB_R = 0,		
		IXOR_R = 1,		
		IADD_RS = 2,	
		IMUL_R = 3,
		IMULH_R = 4,	
		ISMULH_R = 5,	
		IMUL_RCP = 6,
        IADD_M = 7,
        ISUB_M = 8,
		IMUL_M = 9,
		IMULH_M = 10,
		ISMULH_M = 11,
		INEG_R = 12,
		IXOR_M = 13,
		IROR_R = 14,
		IROL_R = 15,
		ISWAP_R = 16,
		FSWAP_R = 17,
		FADD_R = 18,
		FADD_M = 19,
		FSUB_R = 20,
		FSUB_M = 21,
		FSCAL_R = 22,
		FMUL_R = 23,
		FDIV_M = 24,
		FSQRT_R = 25,
		CBRANCH = 26,
		CFROUND = 27,
		ISTORE = 28,
	};

	// Holds instruction's bytecode for given opcode (array index is equal opcode)
	// Built according to frequencies: https://github.com/tevador/RandomX/blob/master/doc/specs.md#5-instruction-set
	inline constexpr std::array<Bytecode, 256> LUT_Opcode = []() consteval {
		std::array<Bytecode, 256> LUT_Opcode_{};
        constexpr std::array<std::pair<Bytecode, uint32_t>, 29>  opcode_frequencies{
            std::pair{ Bytecode::IADD_RS, 16 }, { Bytecode::IADD_M, 7 }, { Bytecode::ISUB_R, 16 },
            { Bytecode::ISUB_M, 7 }, { Bytecode::IMUL_R, 16 }, { Bytecode::IMUL_M, 4 },
            { Bytecode::IMULH_R, 4 }, { Bytecode::IMULH_M, 1 }, { Bytecode::ISMULH_R, 4 },
            { Bytecode::ISMULH_M, 1 }, { Bytecode::IMUL_RCP, 8 }, { Bytecode::INEG_R, 2 },
            { Bytecode::IXOR_R, 15 }, { Bytecode::IXOR_M, 5 }, { Bytecode::IROR_R, 8 },
            { Bytecode::IROL_R, 2 }, { Bytecode::ISWAP_R, 4 }, { Bytecode::FSWAP_R, 4 },
            { Bytecode::FADD_R, 16 }, { Bytecode::FADD_M, 5 }, { Bytecode::FSUB_R, 16 },
            { Bytecode::FSUB_M, 5 }, { Bytecode::FSCAL_R, 6 }, { Bytecode::FMUL_R, 32 },
            { Bytecode::FDIV_M, 4 }, { Bytecode::FSQRT_R, 6 }, { Bytecode::CBRANCH, 25 },
            { Bytecode::CFROUND, 1 }, { Bytecode::ISTORE, 16 },
        };

		uint32_t counter{ 0 };
		for (const auto& [code, freq] : opcode_frequencies) {
			for (uint32_t j = 0; j < freq; ++j) {
				LUT_Opcode_[counter++] = code;
			}
		}

		if (counter != 256) {
			throw "frequencies have to sum up to 256";
		}

		return LUT_Opcode_;
	}();
}

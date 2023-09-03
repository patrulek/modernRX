#pragma once

/*
* Definitions of all instructions in RandomX programs: https://github.com/tevador/RandomX/blob/master/doc/specs.md#5-instruction-set
* This is used by RandomX program interpreter.
*/

#include <array>

#include "randomxparams.hpp"

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
            std::pair{ Bytecode::IADD_RS, Rx_Freq_Iadd_Rs }, { Bytecode::IADD_M, Rx_Freq_Iadd_M}, { Bytecode::ISUB_R, Rx_Freq_Isub_R },
            { Bytecode::ISUB_M, Rx_Freq_Isub_M }, { Bytecode::IMUL_R, Rx_Freq_Imul_R }, { Bytecode::IMUL_M, Rx_Freq_Imul_M },
            { Bytecode::IMULH_R, Rx_Freq_Imulh_R }, { Bytecode::IMULH_M, Rx_Freq_Imulh_M }, { Bytecode::ISMULH_R, Rx_Freq_Ismulh_R },
            { Bytecode::ISMULH_M, Rx_Freq_Ismulh_M }, { Bytecode::IMUL_RCP, Rx_Freq_Imul_Rcp }, { Bytecode::INEG_R, Rx_Freq_Ineg_R },
            { Bytecode::IXOR_R, Rx_Freq_Ixor_R }, { Bytecode::IXOR_M, Rx_Freq_Ixor_M }, { Bytecode::IROR_R, Rx_Freq_Iror_R },
            { Bytecode::IROL_R, Rx_Freq_Irol_R }, { Bytecode::ISWAP_R, Rx_Freq_Iswap_R }, { Bytecode::FSWAP_R, Rx_Freq_Fswap_R },
            { Bytecode::FADD_R, Rx_Freq_Fadd_R }, { Bytecode::FADD_M, Rx_Freq_Fadd_M }, { Bytecode::FSUB_R, Rx_Freq_Fsub_R },
            { Bytecode::FSUB_M, Rx_Freq_Fsub_M }, { Bytecode::FSCAL_R, Rx_Freq_Fscal_R }, { Bytecode::FMUL_R, Rx_Freq_Fmul_R },
            { Bytecode::FDIV_M, Rx_Freq_Fdiv_M }, { Bytecode::FSQRT_R, Rx_Freq_Fsqrt_R }, { Bytecode::CBRANCH, Rx_Freq_Cbranch },
            { Bytecode::CFROUND, Rx_Freq_Cfround }, { Bytecode::ISTORE, Rx_Freq_Istore },
        };

		uint32_t counter{ 0 };
		for (const auto& [code, freq] : opcode_frequencies) {
			for (uint32_t j = 0; j < freq; ++j) {
				LUT_Opcode_[counter++] = code;
			}
		}

		return LUT_Opcode_;
	}();
}

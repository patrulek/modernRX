#pragma once


/*
* Single-threaded and no avx supported implementation of Superscalar program generator based on: https://github.com/tevador/RandomX/blob/master/doc/specs.md#6-superscalarhash
* This is used by RandomX to generate read only dataset.
* Uses Blake2bRandom as a source of random numbers.
*/

#include <array>
#include <optional>

#include "instructionset.hpp"
#include "randomxparams.hpp"
#include "blake2brandom.hpp"


namespace modernRX {
	using reg_idx_t = uint8_t;

	// Holds state for a single superscalar instruction.
	struct Instruction {
		const InstructionInfo* info{ &isa[static_cast<uint8_t>(InstructionType::INVALID)] }; // Instruction template.
		uint8_t op_index{ 0 }; // Current macro operation index to issue.

		std::optional<reg_idx_t> src_register{ std::nullopt }; // Source register index. (nullopt = no source register used).
		reg_idx_t dst_register{ 0 }; // Destination register index. Valid instruction must always point to some register.
		std::optional<uint32_t> src_value{ std::nullopt }; // Instruction source value. (nullopt = constant, 0-7 = src_register)

		uint32_t imm32{ 0 }; // Immediate (constant) 32-bit value.
		uint8_t mod{ 0 }; // Used to modify source register value.

		// Returns numbers of bits from mod used for shifting.
		uint8_t modShift() const {
			return (mod >> 2) % 4;
		}

		// Returns true if current op_index points to invalid macro-op.
		// Should be used to check if instruction was fully issued or was invalidated.
		bool issued() const { return info->ops[op_index].size == 0; }

		// Returns current macro-op and increments its macro-op index.
		std::pair<const MacroOp&, const uint8_t> nextOp() {
			std::pair<const MacroOp&, const uint8_t> res{ std::make_pair(info->ops[op_index], op_index) };
			op_index++;

			return res;
		}

		// Invalidates instruction by seting its template to INVALID type.
		void invalidate() { info = &isa[static_cast<uint8_t>(InstructionType::INVALID)]; };

		// Wrapper function over InstructionInfo.
		InstructionType type() const { return info->type; }

		// Wrapper function over InstructionInfo.
		InstructionType group() const { return info->group; }

		// Wrapper function over InstructionInfo.
		bool srcRegisterAsSrcValue() const { return info->src_register_as_src_value; };

		// Wrapper function over InstructionInfo.
		bool dstRegisterAsSrcRegister() const { return info->dst_register_as_src_register; };

		// Wrapper function over InstructionInfo.
		std::optional<uint8_t> srcOpIndex() const { return info->src_op_index; };

		// Wrapper function over InstructionInfo.
		std::optional<uint8_t> dstOpIndex() const { return info->dst_op_index; };
		
	};

	// Holds information about macro-op combinations that sizes sums up to 16 bytes.
	using DecodeBuffer = std::array<uint32_t, 4>;

	// Holds information about generated superscalar program.
	struct Program {
		std::array<Instruction, Rx_Superscalar_Max_Program_Size> instructions{}; // Superscalar instructions.
		uint32_t size{ 0 }; // Number of instructions in buffer.
		reg_idx_t address_register{ 0 }; // Address register used for dataset generation.
	};

	class Superscalar {
	public:
		// Creates new superscalar program generator and initializes random source with blakeRNG parameter.
		explicit Superscalar(blake2b::Random& blakeRNG);

		// Generates superscalar program based on rules defined in https://github.com/tevador/RandomX/blob/master/doc/specs.md#6-superscalarhash.
		// Resulting program dependes on internal state of blakeRNG random generator.
		void generate(Program& prog);
	private:
		// Return decode buffer configuration, which defines 3 or 4 macro operation slots such that the size of each group is exactly 16 bytes.
		// Rules for selection are defined in: https://github.com/tevador/RandomX/blob/master/doc/specs.md#631-decoding-stage
		// In a case when random selection is needed, blakeRNG is used to provide random values.
		// 
		// instr defines last fetched instruction.
		// decode_cycle defines current decoding cycle number.
		// mul_count defines number of currently fetched IMUL* instructions.
		const DecodeBuffer& selectDecodeBuffer(const InstructionType instr, const uint32_t decode_cycle, const uint32_t mul_count);

		// Return an instruction that should be fetched next based on given decode_buffer and index pointing to the buffer slot (which holds instruction size in bytes).
		// Rules for selection are defined in: https://github.com/tevador/RandomX/blob/master/doc/specs.md#632-instruction-selection
		// In a case when given slot may be occupied by several instructions, blakeRNG is used to randomly select next instruction.
		InstructionType selectInstructionTypeForDecodeBuffer(const DecodeBuffer& decode_buffer, const uint32_t buffer_index);

		// Initializes instructions depending on a given type
		// according to rules from table 6.1.1: https://github.com/tevador/RandomX/blob/master/doc/specs.md#61-instructions
		Instruction initializeInstruction(const InstructionType type);

		// Randomly selects a register index from passed available registers.
		uint8_t selectRegister(const std::vector<reg_idx_t>& available_registers);

		blake2b::Random blakeRNG;
	};

	// Executes given program using provided registers.
	void executeSuperscalar(std::span<uint64_t, 8> registers, const Program& program);
}
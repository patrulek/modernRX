#include <vector>

#include "reciprocal.hpp"
#include "superscalar.hpp"

namespace modernRX {
	namespace {
		constexpr reg_idx_t Register_Needs_Displacement{ 5 }; // This register cannot be destination for IADD_RS.
		constexpr uint32_t Rx_Superscalar_Op_Max_Latency{ maxOpLatency(isa) }; // Maximum latency of all instructions (in cycles of reference CPU).
		constexpr uint32_t Rx_Superscalar_Max_Schedule_Cycle{ Rx_Superscalar_Latency + Rx_Superscalar_Op_Max_Latency }; // Maximum number of cycles in a schedule.

		// Definition of table 6.3.1 from https://github.com/tevador/RandomX/blob/master/doc/specs.md#631-decoding-stage
		constexpr std::array<DecodeBuffer, 6> Decode_Buffers{
			DecodeBuffer{ 4, 8,  4, 0 },
			DecodeBuffer{ 7, 3,  3, 3 },
			DecodeBuffer{ 3, 7,  3, 3 },
			DecodeBuffer{ 4, 9,  3, 0 },
			DecodeBuffer{ 4, 4,  4, 4 },
			DecodeBuffer{ 3, 3, 10, 0 },
		};

		// Holds information about port busyness at given cycle.
		// This has size of 8 instead of 3 (which is number of unique ports used for scheduling), 
		// to not bother with mapping given ExecutionPort to its proper index.
		using PortsSchedule = std::array<std::array<bool, Rx_Superscalar_Max_Schedule_Cycle>, 8>;

		// Holds information about single register.
		struct Register {
			uint32_t availability_cycle{ 0 }; // Which cycle the register will be ready.
			std::optional<uint32_t> last_src_value{ std::nullopt }; // The last operation source value (nullopt = constant, 0-7 = register).
			SuperscalarInstructionType last_group{ SuperscalarInstructionType::INVALID }; // The last operation that was applied to the register.
		};

		// Holds information about all registers.
		using RegisterFile = std::array<Register, Register_Count>;

		// Holds information about simulated ASIC latencies for every register and selected address register.
		struct AsicContext {
			std::array<uint32_t, Register_Count> latencies{}; // Estimated latencies for every register.
			uint32_t max_latency{ 0 }; // Max estimated latency over all registers.
			reg_idx_t max_latency_register{ 0 }; // Register index that holds max estimated latency. Used as address reigster for superscalar program.
		};

		// Holds information about current state of generated program.
		struct ProgramContext {
			uint32_t throwaway_count{ 0 }; // Increases when current instruction dont fit in decode buffer. Resets after proper decode buffer slot filling.
			uint32_t mul_count{ 0 }; // Number of multiplication instructions in program.
			uint32_t cycle{ 0 }; // Simulated CPU cycle. Greater-equal than current decode cycle.
			uint32_t dependency_cycle{ 0 }; // Macro-op scheduled cycle + its latency.
			uint32_t decode_cycle{ 0 }; // Current decode cycle.
			uint32_t program_size{ 0 }; // Number of superscalar instructions in program buffer.
			bool ports_saturated{ false }; // Changed to true when no other macro-op could be scheduled.

			// Returns true if generator stopping criteria is met.
			[[nodiscard]] bool done() const noexcept {
				return ports_saturated || decode_cycle >= Rx_Superscalar_Latency || program_size >= Rx_Superscalar_Max_Program_Size;
			}

			// Advances program context to next cycle.
			void advance() noexcept {
				++decode_cycle;
				++cycle;
			}
		};


		[[nodiscard]] std::pair<std::array<ExecutionPort, 2>, std::optional<uint32_t>> scheduleOp(const PortsSchedule& ports, const MacroOp& op, const uint32_t cycle, const uint32_t dependency_cycle) noexcept;
		[[nodiscard]] std::pair<ExecutionPort, std::optional<uint32_t>> scheduleUop(const PortsSchedule& ports, const ExecutionPort uop_port, const uint32_t cycle) noexcept;

		void findAvailableRegisters(std::vector<reg_idx_t>& available_registers, const RegisterFile& registers, const uint32_t cycle) noexcept;
		[[nodiscard]] bool needRegisterDisplacement(const_span<reg_idx_t> available_register) noexcept;
		void updateAsicContext(AsicContext& asic_ctx, const SuperscalarInstruction& instr) noexcept;
	}

	Superscalar::Superscalar(const blake2b::Random& blakeRNG) noexcept
		: blakeRNG(blakeRNG) {}

	SuperscalarProgram Superscalar::generate() {
		constexpr uint32_t Max_Throwaway_Count{ 256 }; // Number of max tries in case when instruction dont fit in decode buffer.

		// Available registers holds indexes of registers ready at a given CPU cycle.
		std::vector<reg_idx_t> available_registers;
		available_registers.reserve(Register_Count);

		SuperscalarProgram prog{};
		PortsSchedule ports{};
		RegisterFile registers{};
		AsicContext asic_ctx{};
		SuperscalarInstruction instruction{};

		for (ProgramContext ctx{}; !ctx.done(); ctx.advance()) {
			// Each decode cycle decodes 16 bytes of x86 code.
			const auto& decode_buffer{ selectDecodeBuffer(instruction.type(), ctx.decode_cycle, ctx.mul_count) };

			// Select instruction for every non-zero value slot in buffer.
			for (uint32_t decode_buffer_slot = 0; decode_buffer_slot < decode_buffer.size() && decode_buffer[decode_buffer_slot] > 0; ++decode_buffer_slot) {
				const auto top_cycle{ ctx.cycle };

				// Select new instruction if previous already issued (all its macro ops are already scheduled).
				if (instruction.issued()) {
					if (ctx.done()) {
						return prog; // Termination condition reached.
					}
					
					const auto instruction_type{ selectInstructionTypeForDecodeBuffer(decode_buffer, decode_buffer_slot) };
					instruction = initializeInstruction(instruction_type);
				}

				const auto& [op, op_index] { instruction.nextOp() };

				// Calculate the earliest cycle when this macro-op (all of its uOPs) could be scheduled for execution.
				const auto& [schedule_ports, min_schedule_cycle] { scheduleOp(ports, op, ctx.cycle, ctx.dependency_cycle) };
				if (!min_schedule_cycle.has_value()) {
					return prog; // Cannot schedule instr anymore, ports are saturated.
				}
				auto schedule_cycle{ min_schedule_cycle.value() };

				// Precheck: if there are not available registers at schedule_cycle + Rx_Max_Op_Latency
				// no need to check source/dest register availability anyway.
				if (op_index == instruction.srcOpIndex() || op_index == instruction.dstOpIndex()) {
					// Do not check registers that would exceed program limits.
					const uint32_t future_schedule_cycle{ std::min(schedule_cycle + Rx_Superscalar_Op_Max_Latency - 1, Rx_Superscalar_Max_Schedule_Cycle - 1) };
					findAvailableRegisters(available_registers, registers, future_schedule_cycle);

					if (available_registers.size() == 0) {
						ctx.cycle += 4;

						if (ctx.throwaway_count < Max_Throwaway_Count) {
							instruction.invalidate();
							--decode_buffer_slot; // SuperscalarInstruction invalidated, but we dont want to skip current buffer slot.
							++ctx.throwaway_count;

							// Try another instruction for this slot.
							continue;
						}

						// Max throwaway count reached for current slot.
						// Abort this decode buffer completely and go to next decode cycle.
						instruction.invalidate();
						break;
					}
				}

				// Find a source register (if applicable) that will be ready when this instruction executes
				// according to rules in: https://github.com/tevador/RandomX/blob/master/doc/specs.md#634-operand-assignment
				if (op_index == instruction.srcOpIndex()) {
					for (uint32_t forward = 0; !instruction.src_register.has_value() && forward < Rx_Superscalar_Op_Max_Latency; ++forward) {
						findAvailableRegisters(available_registers, registers, schedule_cycle + forward);

						// No available registers, try next cycle.
						if (available_registers.empty()) {
							++ctx.cycle;
							++schedule_cycle;
							continue;
						}

						// If there are only 2 available registers for IADD_RS and one of them is r5, select it as the source because it cannot be the destination.
						// Check rules in table 6.1.1: https://github.com/tevador/RandomX/blob/master/doc/specs.md#61-instructions
						if (instruction.type() == SuperscalarInstructionType::IADD_RS && needRegisterDisplacement(available_registers)) {
							instruction.src_value = instruction.src_register = Register_Needs_Displacement;
							break;
						}

						instruction.src_register = selectRegister(available_registers);
						instruction.src_value = instruction.srcRegisterAsSrcValue() ? instruction.src_register : std::nullopt;
					}
				}


				// Find a destination register that will be ready when this instruction executes
				// according to rules in: https://github.com/tevador/RandomX/blob/master/doc/specs.md#634-operand-assignment
				if (op_index == instruction.dstOpIndex()) {
					for (uint32_t forward = 0; forward < Rx_Superscalar_Op_Max_Latency; ++forward) {
						available_registers.clear();

						for (reg_idx_t i = 0; i < registers.size(); ++i) {
							// Register must be available to use at given cycle.
							if (registers[i].availability_cycle > schedule_cycle) {
								continue;
							}

							// R5 cannot be used as destination for IADD_RS.
							if (instruction.type() == SuperscalarInstructionType::IADD_RS && i == Register_Needs_Displacement) {
								continue;
							}

							// Cannot be the same as src_register unless instruction allows for that.
							if (i == instruction.src_register && !instruction.dstRegisterAsSrcRegister()) {
								continue;
							}

							// Cannot perform current operation on given register
							// if it belongs to the same group and is using the same src_value as a previous operation on this register.
							if (registers[i].last_group == instruction.group() && registers[i].last_src_value == instruction.src_value) {
								continue;
							}

							// Cannot perform another multiplication on given register unless throwaway counter is greater than 0.
							if (ctx.throwaway_count == 0 && instruction.group() == SuperscalarInstructionType::IMUL_R && registers[i].last_group == SuperscalarInstructionType::IMUL_R) {
								continue;
							}

							available_registers.push_back(i);
						}

						// No available registers, try next cycle.
						if (available_registers.empty()) {
							++ctx.cycle;
							++schedule_cycle;
							continue;
						}

						instruction.dst_register = selectRegister(available_registers);
						break;
					}
				}

				ctx.throwaway_count = 0;

				// Recalculate when the instruction can be scheduled for execution based on operand availability.
				const auto& [recalculated_schedule_ports, recalculated_schedule_cycle] { scheduleOp(ports, op, schedule_cycle, ctx.dependency_cycle) };
				if (!recalculated_schedule_cycle.has_value()) {
					return prog; // Cannot schedule instr anymore, ports are saturated.
				}
				schedule_cycle = recalculated_schedule_cycle.value();

				// Update port availability.
				ports[static_cast<uint8_t>(recalculated_schedule_ports[0])][schedule_cycle] = true;
				ports[static_cast<uint8_t>(recalculated_schedule_ports[1])][schedule_cycle] = true;

				// Calculate when the result will be ready.
				ctx.dependency_cycle = schedule_cycle + op.latency;

				// If this instruction writes the result, modify destination register information.
				if (op_index == instruction.resultOpIndex()) {
					Register& reg{ registers[instruction.dst_register] };
					reg.availability_cycle = ctx.dependency_cycle; // At this point dependency_cycle is equal to retire cycle.
					reg.last_group = instruction.group();
					reg.last_src_value = instruction.src_value;
				}

				ctx.cycle = top_cycle;
				ctx.ports_saturated |= (schedule_cycle >= Rx_Superscalar_Latency);

				// When all macro-ops of the current instruction have been issued, vadd instruction into the program.
				if (instruction.issued()) {
					prog.instructions[ctx.program_size++] = instruction;
					prog.size = ctx.program_size;

					updateAsicContext(asic_ctx, instruction);
					prog.address_register = asic_ctx.max_latency_register;

					ctx.mul_count += isMultiplication(instruction.type());
				}
			}
		}

		return prog;
	}

	const DecodeBuffer& Superscalar::selectDecodeBuffer(const SuperscalarInstructionType type, const uint32_t decode_cycle, const uint32_t mul_count) noexcept {
		// If the current RandomX instruction is "IMULH", the next fetch configuration must be 3-3-10
		// because the full 128-bit multiplication instruction is 3 bytes long and decodes to 2 uOPs on Intel CPUs.
		// Intel CPUs can decode at most 4 uOPs per cycle, so this requires a 2-1-1 configuration for a total of 3 macro ops.
		if (type == SuperscalarInstructionType::IMULH_R || type == SuperscalarInstructionType::ISMULH_R) {
			return Decode_Buffers[5];
		}

		// To make sure that the multiplication port is saturated, a 4-4-4-4 configuration is generated if the number of multiplications
		// is lower than the number of cycles.
		if (mul_count < decode_cycle + 1) {
			return Decode_Buffers[4];
		}

		// If the current RandomX instruction is "IMUL_RCP", the next buffer must begin with a 4-byte slot for multiplication.
		if (type == SuperscalarInstructionType::IMUL_RCP) {
			return blakeRNG.getUint8() % 2 ? Decode_Buffers[0] : Decode_Buffers[3];
		}

		// Default: select a random fetch configuration (pick one of: 0, 1, 2, 3).
		return Decode_Buffers[blakeRNG.getUint8() % 4];
	}

	SuperscalarInstructionType Superscalar::selectInstructionTypeForDecodeBuffer(const DecodeBuffer& decode_buffer, const uint32_t buffer_index) noexcept {
		static constexpr std::array<SuperscalarInstructionType, 4> slot_3{ SuperscalarInstructionType::ISUB_R, SuperscalarInstructionType::IXOR_R, SuperscalarInstructionType::IMULH_R, SuperscalarInstructionType::ISMULH_R };
		static constexpr std::array<SuperscalarInstructionType, 2> slot_4{ SuperscalarInstructionType::IROR_C, SuperscalarInstructionType::IADD_RS };
		static constexpr std::array<SuperscalarInstructionType, 2> slot_7{ SuperscalarInstructionType::IXOR_C7, SuperscalarInstructionType::IADD_C7 };
		static constexpr std::array<SuperscalarInstructionType, 2> slot_8{ SuperscalarInstructionType::IXOR_C8, SuperscalarInstructionType::IADD_C8 };
		static constexpr std::array<SuperscalarInstructionType, 2> slot_9{ SuperscalarInstructionType::IXOR_C9, SuperscalarInstructionType::IADD_C9 };

		// Not all decode buffer configurations contain 4 slots, but for simplicity it is implemented
		// as an array of 4 elements, instead of vector with variable size, thus second condition is needed.
		const bool is_last_index{ (buffer_index + 1 == decode_buffer.size()) || (decode_buffer[buffer_index + 1] == 0) };

		switch (decode_buffer[buffer_index]) {
		case 3:
			if (is_last_index) {
				// If its last index in buffer, it can also select `IMULH` instructions, thus modulo 4.
				return slot_3[blakeRNG.getUint8() % 4];
			}

			// For any other buffer index it can only select between ISUB and IXOR.
			return slot_3[blakeRNG.getUint8() % 2];
		case 4:
			// If this is the 4-4-4-4 buffer, issue multiplications as the first 3 instructions.
			if (decode_buffer == Decode_Buffers[4] && !is_last_index) {
				return SuperscalarInstructionType::IMUL_R;
			}

			return slot_4[blakeRNG.getUint8() % 2];
		case 7:
			return slot_7[blakeRNG.getUint8() % 2];
		case 8:
			return slot_8[blakeRNG.getUint8() % 2];
		case 9:
			return slot_9[blakeRNG.getUint8() % 2];
		case 10:
			return SuperscalarInstructionType::IMUL_RCP;
		default:
			std::unreachable();
		}
	}

	SuperscalarInstruction Superscalar::initializeInstruction(const SuperscalarInstructionType type) noexcept {
		SuperscalarInstruction instruction{
			.info{ &isa[static_cast<uint8_t>(type)] },
		};

		switch (type) {
		case SuperscalarInstructionType::ISUB_R: [[fallthrough]];
		case SuperscalarInstructionType::IXOR_R: [[fallthrough]];
		case SuperscalarInstructionType::IMUL_R: [[fallthrough]];
		case SuperscalarInstructionType::INVALID: // Do nothing for these instructions.
			break;
		case SuperscalarInstructionType::IADD_RS:
			instruction.mod = blakeRNG.getUint8();
			break;
		case SuperscalarInstructionType::IROR_C:
			do { instruction.imm32 = blakeRNG.getUint8() % 64; } while (instruction.imm32 == 0);
			break;
		case SuperscalarInstructionType::IADD_C7: [[fallthrough]];
		case SuperscalarInstructionType::IADD_C8: [[fallthrough]];
		case SuperscalarInstructionType::IADD_C9: [[fallthrough]];
		case SuperscalarInstructionType::IXOR_C7: [[fallthrough]];
		case SuperscalarInstructionType::IXOR_C8: [[fallthrough]];
		case SuperscalarInstructionType::IXOR_C9:
			instruction.imm32 = blakeRNG.getUint32();
			break;
		case SuperscalarInstructionType::IMULH_R: [[fallthrough]];
		case SuperscalarInstructionType::ISMULH_R:
			// For some reason it takes 4 bytes, instead of 1 from blake2b generator (which would be suitable for picking register idx).
			// Additionally for other instructions src_value defines only if instruction use constant value or value from one of registers as a source
			// and is used for choosing destination register, but not used during execution.
			// However, for some reason it may assign any uint32 value to src_value, not only those in range 0-7,
			// but greater values are invalid (as register indexes) anyway. Those greater values cannot be treated
			// as a single constant source (thus nullopt like in other instructions), because it could possibly 
			// change behaviour in deciding about destination register.
			//
			// This seems like an overlook in original implementation or i dont understand something.
			// Anyway it has to stay this way.
			instruction.src_value = blakeRNG.getUint32(); 
			break;
		case SuperscalarInstructionType::IMUL_RCP:
			do { instruction.imm32 = blakeRNG.getUint32(); } while (instruction.imm32 == 0 || std::has_single_bit(instruction.imm32));
			instruction.reciprocal = reciprocal(instruction.imm32);
			break;
		default:
			std::unreachable();
		}

		return instruction;
	}

	uint8_t Superscalar::selectRegister(const_span<reg_idx_t> available_registers) noexcept {
		return available_registers.size() == 1 ? available_registers[0] : available_registers[blakeRNG.getUint32() % available_registers.size()];
	}

	namespace {
		// Returns up to two ExecutionPort and a cycle for which given macro-op should be scheduled according to rules: https://github.com/tevador/RandomX/blob/master/doc/specs.md#633-port-assignment
		std::pair<std::array<ExecutionPort, 2>, std::optional<uint32_t>> scheduleOp(const PortsSchedule& ports, const MacroOp& op, const uint32_t cycle, const uint32_t dependency_cycle) noexcept {
			// Operations that does not require any execution port are eliminated, eg. MOV R,R does not need to occupy any execution unit (mov elimination).
			if (!op.requiresPort()) {
				return std::make_pair(std::array<ExecutionPort, 2>{ ExecutionPort::NONE, ExecutionPort::NONE }, cycle);
			}

			// If this macro-op depends on the previous one, increase the starting cycle if needed.
			// Only IMUL_RCP are dependent and this handles an explicit dependency chain in IMUL_RCP.
			uint32_t schedule_cycle{ op.dependent ? std::max(cycle, dependency_cycle) : cycle };

			// Operations with single uOp need to check availability of only one execution port.
			if (!op.fused()) {
				const auto& [uop_port, uop_schedule_cycle] { scheduleUop(ports, op.ports[0], schedule_cycle) };
				return std::make_pair(std::array<ExecutionPort, 2>{ uop_port, ExecutionPort::NONE }, uop_schedule_cycle);
			}

			// Macro-ops with 2 uOps are scheduled conservatively by requiring both uOps to execute in the same cycle
			while (schedule_cycle < Rx_Superscalar_Max_Schedule_Cycle) {
				const auto& [uop1_port, uop1_schedule_cycle] { scheduleUop(ports, op.ports[0], schedule_cycle) };
				const auto& [uop2_port, uop2_schedule_cycle] { scheduleUop(ports, op.ports[1], schedule_cycle) };

				if (uop1_schedule_cycle != uop2_schedule_cycle || !uop1_schedule_cycle.has_value()) {
					++schedule_cycle;
					continue;
				}

				return std::make_pair(std::array<ExecutionPort, 2>{ uop1_port, uop2_port }, uop1_schedule_cycle);
			}

			return std::make_pair(std::array<ExecutionPort, 2>{ ExecutionPort::NONE, ExecutionPort::NONE }, std::nullopt);
		}

		// Return single ExecutionPort and cycle for which given uOp should be scheduled according to rules: https://github.com/tevador/RandomX/blob/master/doc/specs.md#633-port-assignment
		// The scheduling here is done optimistically by checking port availability in order P5 -> P0 -> P1 to not overload
		// port P1 (multiplication) by instructions that can go to any port.
		std::pair<ExecutionPort, std::optional<uint32_t>> scheduleUop(const PortsSchedule& ports, const ExecutionPort uop_port, const uint32_t cycle) noexcept {
			auto schedule_cycle{ cycle };

			const auto canScheduleToPort = [&](const ExecutionPort port) noexcept {
				const bool can_schedule{ (static_cast<uint8_t>(port) & static_cast<uint8_t>(uop_port)) != static_cast<uint8_t>(ExecutionPort::NONE) };
				const bool is_busy{ ports[static_cast<uint8_t>(port)][schedule_cycle] };

				return can_schedule && !is_busy;
			};

			while (schedule_cycle < Rx_Superscalar_Max_Schedule_Cycle) {
				if (canScheduleToPort(ExecutionPort::P5)) {
					return std::make_pair(ExecutionPort::P5, schedule_cycle);
				}

				if (canScheduleToPort(ExecutionPort::P0)) {
					return std::make_pair(ExecutionPort::P0, schedule_cycle);
				}

				if (canScheduleToPort(ExecutionPort::P1)) {
					return std::make_pair(ExecutionPort::P1, schedule_cycle);
				}

				++schedule_cycle;
			}

			return std::make_pair(ExecutionPort::NONE, std::nullopt);
		}

		// Fills available_registers vector with registers indexes that are ready for given cycle (included).
		// Used to refresh list of ready registers before selecting one as a source or desination.
		void findAvailableRegisters(std::vector<reg_idx_t>& available_registers, const RegisterFile& registers, const uint32_t cycle) noexcept {
			available_registers.clear();

			for (reg_idx_t i = 0; i < registers.size(); ++i) {
				if (registers[i].availability_cycle <= cycle) {
					available_registers.push_back(i);
				}
			}
		}

		// Returns true if there are only two available registers and one of them cannot be selected as a destination register.
		// Used to select proper source register.
		bool needRegisterDisplacement(const_span<reg_idx_t> available_registers) noexcept {
			return available_registers.size() == 2 && (available_registers[0] == Register_Needs_Displacement || available_registers[1] == Register_Needs_Displacement);
		}
		
		// Should be called immediately after last macro-op of given instruction was issued.
		void updateAsicContext(AsicContext& asic_ctx, const SuperscalarInstruction& instr) noexcept {
			// Pick max latency between source and destination.
			const auto src_register{ instr.src_register.has_value() ? instr.src_register.value() : instr.dst_register};
			const uint32_t dst_latency{ asic_ctx.latencies[instr.dst_register] + 1 };
			const uint32_t src_latency{ instr.dst_register != src_register ? asic_ctx.latencies[src_register] + 1 : 0 };
			asic_ctx.latencies[instr.dst_register] = std::max(dst_latency, src_latency);

			// Update max latency register.
			const bool greater_latency{ asic_ctx.latencies[instr.dst_register] > asic_ctx.max_latency };
			const bool equal_latency_and_lesser_idx{ asic_ctx.latencies[instr.dst_register] == asic_ctx.max_latency && instr.dst_register < asic_ctx.max_latency_register };

			if (greater_latency || equal_latency_and_lesser_idx) {
				asic_ctx.max_latency_register = instr.dst_register;
				asic_ctx.max_latency = asic_ctx.latencies[instr.dst_register];
			}
		};
	}
}
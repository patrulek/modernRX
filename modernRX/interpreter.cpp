#include <array>
#include <utility>

#include "aes1rhash.hpp"
#include "aes4rrandom.hpp"
#include "bytecode.hpp"
#include "interpreter.hpp"
#include "intrinsics.hpp"
#include "randomxparams.hpp"
#include "reciprocal.hpp"


namespace modernRX {
    namespace {
        constexpr uint32_t Int_Register_Count{ 8 };
        constexpr uint32_t Float_Register_Count{ 4 };

        constexpr uint64_t Mantissa_Size{ 52 };
        constexpr uint64_t Cache_Line_Size{ sizeof(DatasetItem) };
        constexpr uint64_t Cache_Line_Align_Mask{ (Rx_Dataset_Base_Size - 1) & ~(Cache_Line_Size - 1) }; // Dataset 64-byte alignment mask.

        using xmm128d_t = intrinsics::sse::xmm<double>;

        // Holds representation of register file used by interpreter during program execution.
        // https://github.com/tevador/RandomX/blob/master/doc/specs.md#43-registers
        struct RegisterFile {
            std::array<uint64_t, Int_Register_Count> r{}; // Common integer registers. Source or destination of integer instructions. Can be used as address registers for scratchpad access.
            std::array<xmm128d_t, Float_Register_Count> f{}; // "Additive" registers. Destination of floating point addition and substraction instructions.
            std::array<xmm128d_t, Float_Register_Count> e{}; // "Multiplicative" registers. Destination of floating point multiplication, division and square root instructions.
            std::array<xmm128d_t, Float_Register_Count> a{}; // Read-only, fixed-value floating point registers. Source operand of any floating point instruction.
        };

        // Holds memory addresses for program execution.
        // https://github.com/tevador/RandomX/blob/master/doc/specs.md#43-registers
        struct MemoryRegisters {
            uint32_t mx{ 0 }; // Holds memory address of the next Dataset read. Always aligned to be multiple of 64.
            uint32_t ma{ 0 }; // Holds memory address of the next Dataset prefetch. Always aligned to be multiple of 64.
        };

        // Holds configuration of the program.
        // https://github.com/tevador/RandomX/blob/master/doc/specs.md#454-address-registers
        // https://github.com/tevador/RandomX/blob/master/doc/specs.md#455-dataset-offset
        // https://github.com/tevador/RandomX/blob/master/doc/specs.md#456-group-e-register-masks
        struct ProgramConfiguration {
            std::array<uint64_t, 2> e_mask{}; // Used for the conversion of "E-group" registers.
            std::array<uint32_t, 4> read_reg{}; // Used to select 4 address registers for program execution.
            uint64_t dataset_offset{ 0 }; // Used when reading values from the Dataset.
        };

        [[nodiscard]] constexpr double getSmallPositiveFloat(const uint64_t entropy) noexcept;
        [[nodiscard]] constexpr uint64_t getFloatRegisterMask(const uint64_t entropy) noexcept;
        [[nodiscard]] xmm128d_t convertFloatRegister(const xmm128d_t x, const_span<uint64_t, 2> mask) noexcept;
    }

    // Defines single RandomX program instruction: https://github.com/tevador/RandomX/blob/master/doc/specs.md#51-instruction-encoding
    // Initialized by AES-filled buffer. 
    // Must preserve order of fields.
    struct RxInstruction {
        uint8_t opcode; // https://github.com/tevador/RandomX/blob/master/doc/specs.md#511-opcode
        uint8_t dst_register; // https://github.com/tevador/RandomX/blob/master/doc/specs.md#512-dst
        uint8_t src_register; // https://github.com/tevador/RandomX/blob/master/doc/specs.md#513-src
        uint8_t mod; // https://github.com/tevador/RandomX/blob/master/doc/specs.md#514-mod
        uint32_t imm32; // https://github.com/tevador/RandomX/blob/master/doc/specs.md#515-imm32

        // Used to select between Scratchpad levels L1 and L2 when reading from or writing to memory.
        [[nodiscard]] uint8_t modMask() const noexcept {
            return mod % 4;
        }

        // Used by IADD_RS instruction.
        [[nodiscard]] uint8_t modShift() const noexcept {
            return (mod >> 2) % 4;
        }

        // Used by CBRANCH and ISTORE instructions.
        [[nodiscard]] uint8_t modCond() const noexcept {
            return mod >> 4;
        }
    };
    static_assert(sizeof(RxInstruction) == 8, "Size of single instruction must be 8 bytes");

    // Holds RandomX program entropy and instructions: https://github.com/tevador/RandomX/blob/master/doc/specs.md#44-program-buffer
    // Initialized by AES-filled buffer. Must preserve order of fields.
    struct RxProgram {
        std::array<uint64_t, 16> entropy{};
        std::array<RxInstruction, Rx_Program_Size> instructions{};
    };

    struct ProgramContext {
        [[nodiscard]] explicit ProgramContext(const RxProgram& program) noexcept;

        RegisterFile rf{};
        MemoryRegisters mem{};
        MemoryRegisters sp_addr{};
        ProgramConfiguration cfg{};
        std::array<int32_t, Int_Register_Count> reg_usage{};
        uint32_t ic{ 0 };
        uint32_t iter{ 0 };
        std::array<int16_t, Rx_Program_Size> branch_target{};
    };

    std::pair<ProgramContext, RxProgram> Interpreter::generateProgram() {
        RxProgram program{};
        aes::fill4R(span_cast<std::byte>(program), seed);

        // Last 64 bytes of the program are now the new seed.
        std::memcpy(seed.data(), span_cast<std::byte>(program).data() + sizeof(RxProgram) - seed.size(), seed.size());

        return std::make_pair(ProgramContext{ program }, program);
    }

    Interpreter::Interpreter(std::span<std::byte, 64> seed, const std::vector<DatasetItem>& dataset)
        : dataset(dataset), scratchpad(seed) {
        std::copy(seed.begin(), seed.end(), this->seed.begin());
    }

    void Interpreter::executeProgram(ProgramContext& ctx, const RxProgram& program) {
        // https://github.com/tevador/RandomX/blob/master/doc/specs.md#462-loop-execution
        for (ctx.iter = 0; ctx.iter < Rx_Program_Iterations; ++ctx.iter) {
            // This performs steps: 1-3
            initializeRegisters(ctx);

            // This loop performs step: 4
            for (ctx.ic = 0; ctx.ic < program.instructions.size(); ++ctx.ic) {
                const auto& instr{ program.instructions[ctx.ic] };
                executeInstruction(ctx, instr);
            }

            // This performs steps: 5-12
            finalizeRegisters(ctx);
        }
    }

    std::array<std::byte, 32> Interpreter::execute() {
        // RandomX requires specific float environment before executing any program.
        // This RAII object will set proper float flags on creation and restore its values on destruction. 
        const intrinsics::sse::FloatEnvironment fenv{};

        for (uint32_t i = 0; i < Rx_Program_Count - 1; ++i) {
            auto [ctx, program] { generateProgram() };
            executeProgram(ctx, program);
            blake2b::hash(seed, span_cast<std::byte, sizeof(ctx.rf)>(ctx.rf), std::span<std::byte>{});
        }

        auto [ctx, program] { generateProgram() };
        executeProgram(ctx, program);
        aes::hash1R(span_cast<std::byte, sizeof(ctx.rf.a)>(ctx.rf.a), span_cast<std::byte, Rx_Scratchpad_L3_Size>(scratchpad.data()));

        std::array<std::byte, 32> output{};
        blake2b::hash(output, span_cast<std::byte>(ctx.rf), std::span<std::byte>{});

        return output;
    }

    void Interpreter::initializeRegisters(ProgramContext& ctx) {
        constexpr uint32_t Scratchpad_L3_Mask64{ (Rx_Scratchpad_L3_Size - 1) & ~63 }; // L3 cache 64-byte alignment mask.

        // Step 1.
        const uint64_t spMix{ ctx.rf.r[ctx.cfg.read_reg[0]] ^ ctx.rf.r[ctx.cfg.read_reg[1]] };
        ctx.sp_addr.mx ^= spMix;
        ctx.sp_addr.mx &= Scratchpad_L3_Mask64;
        ctx.sp_addr.ma ^= spMix >> 32;
        ctx.sp_addr.ma &= Scratchpad_L3_Mask64;

        // Step 2.
        for (uint32_t i = 0; i < Int_Register_Count; ++i) {
            const auto offset{ ctx.sp_addr.mx + 8 * i };
            ctx.rf.r[i] ^= scratchpad.read(offset);
        }

        // Step 3.
        // -----
        // "F-group" register initialization: https://github.com/tevador/RandomX/blob/master/doc/specs.md#431-group-f-register-conversion
        for (uint32_t i = 0; i < Float_Register_Count; ++i) {
            const auto offset{ ctx.sp_addr.ma + 8 * i };
            ctx.rf.f[i] = intrinsics::sse::vcvtpi32<double>(scratchpad.data() + offset);
        }

        // "E-group" register initialization: https://github.com/tevador/RandomX/blob/master/doc/specs.md#432-group-e-register-conversion
        for (uint32_t i = 0; i < Float_Register_Count; ++i) {
            const auto offset{ ctx.sp_addr.ma + 8 * (4 + i) };
            const auto x{ intrinsics::sse::vcvtpi32<double>(scratchpad.data() + offset) };

            ctx.rf.e[i] = convertFloatRegister(x, ctx.cfg.e_mask);
        }
    }

    void Interpreter::executeInstruction(ProgramContext& ctx, const RxInstruction& instr) {
        constexpr uint32_t Scratchpad_L1_Mask{ (Rx_Scratchpad_L1_Size - 1) & ~7 }; // L1 cache 8-byte alignment mask.
        constexpr uint32_t Scratchpad_L2_Mask{ (Rx_Scratchpad_L2_Size - 1) & ~7 }; // L2 cache 8-byte alignment mask.
        constexpr uint32_t Scratchpad_L3_Mask{ (Rx_Scratchpad_L3_Size - 1) & ~7 }; // L3 cache 8-byte alignment mask.

        // Initialize register indexes. Modulo is used to handle register overflow.
        const auto src_register{ instr.src_register % Int_Register_Count };
        const auto dst_register{ instr.dst_register % Int_Register_Count };
        const auto f_src_register{ instr.src_register % Float_Register_Count };
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };

        // Calculate the offset for the memory instructions.
        const auto offset = [&]() {
            // Check if the instruction is an integer one based on the opcode.
            const auto bc{ static_cast<uint8_t>(LUT_Opcode[instr.opcode]) };
            const bool is_integer_instr{ bc < static_cast<uint8_t>(Bytecode::FSWAP_R) || bc > static_cast<uint8_t>(Bytecode::FSQRT_R) };

            const auto imm{ static_cast<int32_t>(instr.imm32) }; // Two's complement sign extension.

            // L3 cache is used only for integer instructions.
            if (is_integer_instr && src_register == dst_register) {
                return static_cast<uint64_t>(imm) & Scratchpad_L3_Mask;
            }

            const auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };
            const auto src_value{ ctx.rf.r[src_register] };

            return (src_value + imm) & mem_mask;
        }();

        // Source operands.
        uint64_t r_src_value{ ctx.rf.r[src_register] }; // Integer register source value.
        uint64_t m_src_value{ scratchpad.read(offset) }; // Integer memory source value.
        xmm128d_t f_src_value{ intrinsics::sse::vcvtpi32<double>(scratchpad.data() + offset) }; // Float memory source value.

        // Instruction execution: https://github.com/tevador/RandomX/blob/master/doc/specs.md#5-instruction-set
        switch (LUT_Opcode[instr.opcode]) { using enum Bytecode;
        case IADD_RS:
        {
            constexpr uint8_t displacement_reg_idx{ 5 };
            const auto imm{ (dst_register != displacement_reg_idx) ? 0 : static_cast<int32_t>(instr.imm32) };
            ctx.rf.r[dst_register] += (r_src_value << instr.modShift()) + imm;
            break;
        }
        case IADD_M:
            ctx.rf.r[dst_register] += m_src_value;
            break;
        case ISUB_R:
            if (src_register == dst_register) {
                r_src_value = static_cast<int32_t>(instr.imm32);
            }

            ctx.rf.r[dst_register] -= r_src_value;
            break;
        case ISUB_M:
            ctx.rf.r[dst_register] -= m_src_value;
            break;
        case IMUL_R:
            if (src_register == dst_register) {
                r_src_value = static_cast<int32_t>(instr.imm32);
            }

            ctx.rf.r[dst_register] *= r_src_value;
            break;
        case IMUL_M:
            ctx.rf.r[dst_register] *= m_src_value;
            break;
        case IMULH_R:
            ctx.rf.r[dst_register] = intrinsics::umulh(ctx.rf.r[dst_register], r_src_value);
            break;
        case IMULH_M:
            ctx.rf.r[dst_register] = intrinsics::umulh(ctx.rf.r[dst_register], m_src_value);
            break;
        case ISMULH_R:
            ctx.rf.r[dst_register] = intrinsics::smulh(ctx.rf.r[dst_register], r_src_value);
            break;
        case ISMULH_M:
            ctx.rf.r[dst_register] = intrinsics::smulh(ctx.rf.r[dst_register], m_src_value);
            break;
        case IMUL_RCP:
        {
            const uint32_t divisor{ instr.imm32 };
            if (divisor != 0 && !std::has_single_bit(divisor)) {
                r_src_value = reciprocal(divisor);
                ctx.rf.r[dst_register] *= r_src_value;
            }
            break;
        }
        case INEG_R:
            ctx.rf.r[dst_register] = ~(ctx.rf.r[dst_register]) + 1; // Two's complement negative.
            break;
        case IXOR_R:
            if (src_register == dst_register) {
                r_src_value = static_cast<int32_t>(instr.imm32);
            }

            ctx.rf.r[dst_register] ^= r_src_value;
            break;
        case IXOR_M:
            ctx.rf.r[dst_register] ^= m_src_value;
            break;
        case IROR_R:
            if (src_register == dst_register) {
                r_src_value = instr.imm32;
            }

            ctx.rf.r[dst_register] = std::rotr(ctx.rf.r[dst_register], r_src_value % 64);
            break;
        case IROL_R:
            if (src_register == dst_register) {
                r_src_value = instr.imm32;
            }

            ctx.rf.r[dst_register] = std::rotl(ctx.rf.r[dst_register], r_src_value % 64);
            break;
        case ISWAP_R:
            if (src_register != dst_register) {
                std::swap(ctx.rf.r[src_register], ctx.rf.r[dst_register]);
            }
            break;
        case FSWAP_R:
            if (dst_register < Float_Register_Count) {
                ctx.rf.f[f_dst_register] = intrinsics::sse::vswap<double>(ctx.rf.f[f_dst_register]);
                break;
            }

            ctx.rf.e[f_dst_register] = intrinsics::sse::vswap<double>(ctx.rf.e[f_dst_register]);
            break;
        case FADD_R:
            ctx.rf.f[f_dst_register] = intrinsics::sse::vadd<double>(ctx.rf.f[f_dst_register], ctx.rf.a[f_src_register]);
            break;
        case FADD_M:
            ctx.rf.f[f_dst_register] = intrinsics::sse::vadd<double>(ctx.rf.f[f_dst_register], f_src_value);
            break;
        case FSUB_R:
            ctx.rf.f[f_dst_register] = intrinsics::sse::vsub<double>(ctx.rf.f[f_dst_register], ctx.rf.a[f_src_register]);
            break;
        case FSUB_M:
            ctx.rf.f[f_dst_register] = intrinsics::sse::vsub<double>(ctx.rf.f[f_dst_register], f_src_value);
            break;
        case FSCAL_R:
        {
            const auto mask{ intrinsics::sse::vbcasti64<double>(0x80F0000000000000) };
            ctx.rf.f[f_dst_register] = intrinsics::sse::vxor<double>(ctx.rf.f[f_dst_register], mask);
            break;
        }
        case FMUL_R:
            ctx.rf.e[f_dst_register] = intrinsics::sse::vmul<double>(ctx.rf.e[f_dst_register], ctx.rf.a[f_src_register]);
            break;
        case FDIV_M:
        {
            f_src_value = convertFloatRegister(f_src_value, ctx.cfg.e_mask);
            ctx.rf.e[f_dst_register] = intrinsics::sse::vdiv<double>(ctx.rf.e[f_dst_register], f_src_value);
            break;
        }
        case FSQRT_R:
            ctx.rf.e[f_dst_register] = intrinsics::sse::vsqrt<double>(ctx.rf.e[f_dst_register]);
            break;
        case CBRANCH:
        {
            constexpr uint32_t Condition_Mask{ (1 << Rx_Jump_Bits) - 1 };

            const auto shift{ instr.modCond() + Rx_Jump_Offset };
            const auto mem_mask{ Condition_Mask << shift }; 
            
            uint64_t imm{ static_cast<int32_t>(instr.imm32) | (1ULL << shift) };
            if (Rx_Jump_Offset > 0 || shift > 0) { // Clear the bit below the condition mask - this limits the number of successive jumps to 2.
                imm &= ~(1ULL << (shift - 1));
            }

            ctx.rf.r[dst_register] += imm;
            if ((ctx.rf.r[dst_register] & mem_mask) == 0) {
                ctx.ic = ctx.branch_target[ctx.ic];
            }

            break;
        }
        case CFROUND:
        {
            const auto imm{ instr.imm32 % 64 };
            intrinsics::sse::setFloatRoundingMode(std::rotr(r_src_value, imm) % intrinsics::sse::Floating_Round_Modes);
            break;
        }
        case ISTORE:
        {
            constexpr uint32_t l3_store_condition{ 14 };
            const auto imm{ static_cast<int32_t>(instr.imm32) };

            auto mem_mask{ Scratchpad_L3_Mask };
            if (instr.modCond() < l3_store_condition) {
                mem_mask = instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask;
            }

            const auto store_offset{ (ctx.rf.r[dst_register] + imm) & mem_mask };
            scratchpad.write(store_offset, r_src_value);
            break;
        }
        default:
            std::unreachable();
        }
    }

    void Interpreter::finalizeRegisters(ProgramContext& ctx) {
        // Step 5.
        ctx.mem.mx ^= ctx.rf.r[ctx.cfg.read_reg[2]] ^ ctx.rf.r[ctx.cfg.read_reg[3]];
        ctx.mem.mx &= Cache_Line_Align_Mask;

        // Step 6. - omitted
        // Step 7. and step 9.
        const auto dt_index{ (ctx.cfg.dataset_offset + ctx.mem.ma) / Cache_Line_Size };
        DatasetItem dt_item{ dataset[dt_index] };

        for (uint32_t i = 0; i < Int_Register_Count; ++i) {
            const auto offset{ ctx.sp_addr.ma + 8 * i };
            ctx.rf.r[i] ^= dt_item[i];
            scratchpad.write(offset, ctx.rf.r[i]);
        }

        // Step 8.
        std::swap(ctx.mem.mx, ctx.mem.ma);

        // Step 10.
        for (uint32_t i = 0; i < Float_Register_Count; ++i) {
            ctx.rf.f[i] = intrinsics::sse::vxor<double>(ctx.rf.f[i], ctx.rf.e[i]);
        }

        // Step 11.
        for (uint32_t i = 0; i < Float_Register_Count; ++i) {
            const auto offset{ ctx.sp_addr.mx + 16 * i };
            scratchpad.write(offset, std::bit_cast<uint64_t>(ctx.rf.f[i].m128d_f64[0]));
            scratchpad.write(offset + 8, std::bit_cast<uint64_t>(ctx.rf.f[i].m128d_f64[1]));
        }

        // Step 12.
        ctx.sp_addr.mx = 0;
        ctx.sp_addr.ma = 0;
    }

    ProgramContext::ProgramContext(const RxProgram& program) noexcept {
        constexpr uint64_t Dataset_Extra_Items{ Rx_Dataset_Extra_Size / sizeof(DatasetItem) };
        const auto entropy{ program.entropy };
        
        // "A-group" register initialization: https://github.com/tevador/RandomX/blob/master/doc/specs.md#452-group-a-register-initialization
        rf.a[0] = { getSmallPositiveFloat(entropy[0]), getSmallPositiveFloat(entropy[1]) }; 
        rf.a[1] = { getSmallPositiveFloat(entropy[2]), getSmallPositiveFloat(entropy[3]) };
        rf.a[2] = { getSmallPositiveFloat(entropy[4]), getSmallPositiveFloat(entropy[5]) };
        rf.a[3] = { getSmallPositiveFloat(entropy[6]), getSmallPositiveFloat(entropy[7]) };

        // Memory registers initialization: https://github.com/tevador/RandomX/blob/master/doc/specs.md#453-memory-registers
        mem.ma = static_cast<uint32_t>(entropy[8] & Cache_Line_Align_Mask);
        mem.mx = static_cast<uint32_t>(entropy[10]);

        // Scratchpad address registers initialization: https://github.com/tevador/RandomX/blob/master/doc/specs.md#461-initialization
        sp_addr.ma = mem.ma;
        sp_addr.mx = mem.mx;

        // Address registers initialization: https://github.com/tevador/RandomX/blob/master/doc/specs.md#454-address-registers
        for (uint32_t i = 0; i < cfg.read_reg.size(); ++i) {
            cfg.read_reg[i] = (i * 2) + ((entropy[12] >> i) & 1);
        }

        // Dataset offset initialization: https://github.com/tevador/RandomX/blob/master/doc/specs.md#455-dataset-offset
        cfg.dataset_offset = (entropy[13] % (Dataset_Extra_Items + 1)) * Cache_Line_Size;

        // "E-group" register masks initialization: https://github.com/tevador/RandomX/blob/master/doc/specs.md#456-group-e-register-masks
        cfg.e_mask[0] = getFloatRegisterMask(entropy[14]);
        cfg.e_mask[1] = getFloatRegisterMask(entropy[15]);

        reg_usage.fill(-1);
        
        for (uint32_t i = 0; i < program.instructions.size(); ++i) {
            // Getting registers indexes. Modulo is used to handle register overflow.
            const auto dst_register{ program.instructions[i].dst_register % Int_Register_Count };
            const auto src_register{ program.instructions[i].src_register % Int_Register_Count };

            switch (LUT_Opcode[program.instructions[i].opcode]) { using enum Bytecode;
            case IADD_RS: [[fallthrough]];
            case IADD_M: [[fallthrough]];
            case ISUB_R: [[fallthrough]];
            case ISUB_M: [[fallthrough]];
            case IMUL_R: [[fallthrough]];
            case IMUL_M: [[fallthrough]];
            case IMULH_R: [[fallthrough]];
            case IMULH_M: [[fallthrough]];
            case ISMULH_R: [[fallthrough]];
            case ISMULH_M: [[fallthrough]];
            case INEG_R: [[fallthrough]];
            case IXOR_R: [[fallthrough]];
            case IXOR_M: [[fallthrough]];
            case IROR_R: [[fallthrough]];
            case IROL_R:
                reg_usage[dst_register] = i;
                break;
            case IMUL_RCP:
                if (!std::has_single_bit(program.instructions[i].imm32)) {
                    reg_usage[dst_register] = i;
                }
                // No else, because IMUL_RCP with power of 2 is a NOP.
                break;
            case ISWAP_R:
                if (src_register != dst_register) {
                    reg_usage[dst_register] = i;
                    reg_usage[src_register] = i;
                }
                // No else, because ISWAP_R with same registers is a NOP.
                break;
            case CBRANCH:
                branch_target[i] = reg_usage[dst_register];
                for (auto& reg : reg_usage) { reg = i; } // Set all registers as used.
                break;
            default:
                // For all other instructions, do nothing.
                break;
            }
        }
    }

    namespace {
        // Used to initialize "A-group" register values:
        // https://github.com/tevador/RandomX/blob/master/doc/specs.md#452-group-a-register-initialization
        constexpr double getSmallPositiveFloat(const uint64_t entropy) noexcept {
            constexpr uint64_t exponent_size{ 11 };
            constexpr uint64_t mantissa_mask{ (1ULL << Mantissa_Size) - 1 };
            constexpr uint64_t exponent_mask{ (1ULL << exponent_size) - 1 };
            constexpr uint64_t exponent_bias{ 1023 };

            const auto mantissa{ entropy & mantissa_mask };

            auto exponent{ entropy >> 59 }; // 0 .. 31
            exponent += exponent_bias;
            exponent &= exponent_mask;
            exponent <<= Mantissa_Size;

            return std::bit_cast<double>(exponent | mantissa);
        }

        // Used to get "E-group" register masks:
        // https://github.com/tevador/RandomX/blob/master/doc/specs.md#456-group-e-register-masks
        constexpr uint64_t getFloatRegisterMask(const uint64_t entropy) noexcept {
            constexpr uint64_t mask22bits{ (1ULL << 22) - 1 };
            constexpr uint64_t exponent_bits{ 4 };

            uint64_t exponent{ 0b01100000000 }; // Step 2. of: https://github.com/tevador/RandomX/blob/master/doc/specs.md#432-group-e-register-conversion
            exponent |= (entropy >> (64 - exponent_bits)) << exponent_bits;
            exponent <<= Mantissa_Size;

            return (entropy & mask22bits) | exponent;
        }

        // Used to convert "E-group" registers values:
        // https://github.com/tevador/RandomX/blob/master/doc/specs.md#432-group-e-register-conversion
        xmm128d_t convertFloatRegister(const xmm128d_t x, const_span<uint64_t, 2> mask) noexcept {
            constexpr uint64_t Exponent_Bits{ 4 };
            constexpr uint64_t Mantissa_Mask{ (1ULL << (Mantissa_Size + Exponent_Bits)) - 1 };

            const xmm128d_t xmm_mantissa_mask{
                std::bit_cast<double>(Mantissa_Mask),
                std::bit_cast<double>(Mantissa_Mask),
            };

            const xmm128d_t xmm_exponent_mask{
                std::bit_cast<double>(mask[0]),
                std::bit_cast<double>(mask[1]),
            };

            const auto y{ intrinsics::sse::vand<double>(x, xmm_mantissa_mask) };
            return intrinsics::sse::vor<double>(y, xmm_exponent_mask);
        }
    }
}

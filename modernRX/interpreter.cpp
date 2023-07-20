#include "interpreter.hpp"
#include "intrinsics.hpp"
#include "reciprocal.hpp"
#include "aes4rrandom.hpp"
#include "aes1rhash.hpp"
#include "blake2b.hpp"
#include "bytecode.hpp"
#include "logger.hpp"

#include <array>
#include <utility>

namespace modernRX::interpreter {
    namespace {
        constexpr uint64_t Mantissa_Size{ 52 };
        constexpr uint64_t Dynamic_Exponent_Bits{ 4 };
        constexpr uint64_t Dynamic_Mantissa_Mask{ (1ULL << (Mantissa_Size + Dynamic_Exponent_Bits)) - 1 };
        constexpr uint64_t Cache_Line_Align_Mask{ 2'147'483'584 };
        constexpr uint64_t Cache_Line_Size{ 64 };


        constexpr uint64_t getSmallPositiveFloatBits(const uint64_t entropy);
        constexpr uint64_t getStaticExponent(const uint64_t entropy);
        constexpr uint64_t floatMask(const uint64_t entropy);
        constexpr uint64_t signExtend2sCompl(const uint32_t x);
        constexpr int64_t unsigned64ToSigned2sCompl(const uint64_t x);
        F128Reg maskRegisterExponentMantissa(F128Reg x, const ProgramConfiguration& config);


        uint64_t checksum(const void* regfile) {
            uint64_t cs{ 1 };

            for (int i = 0; i < 32 - 1; i++) {
                uint64_t x = *((uint64_t*)regfile + i);
                uint64_t y = *((uint64_t*)regfile + i + 1);
                cs += x;
                cs ^= y;
            }

            return cs;
        }
    }


    RxProgram Interpreter::generateProgram() {
        RxProgram program{};

        aes::Random4R aes4r_random{ seed };
        aes4r_random.fill(stdexp::span_cast<std::byte>(program));

        return program;
    }

    Interpreter::Interpreter(const DatasetMemory& dataset, const std::span<std::byte, 64> seed)
        : dataset(dataset), scratchpad(seed) {

        std::memcpy(this->seed.data(), seed.data(), seed.size());
    }

    std::array<std::byte, 32> Interpreter::run() {
        // RandomX requires specific float environment before executing any program.
        // This RAII object will set proper float flags on creation and restore its values on destruction. 
        const intrinsics::sse::FloatEnvironment fenv{};

        for (uint32_t i = 0; i < 7; ++i) {
            const auto prog{ generateProgram() };
            const auto rf{ execute(prog) };
            blake2b::hash(seed, stdexp::span_cast<std::byte, sizeof(RegisterFile)>(rf), std::span<std::byte>{});
        }

        const auto prog{ generateProgram() };
        const auto rf{ execute(prog) };
        aes::hash1R(stdexp::span_cast<std::byte, sizeof(rf.a)>(rf.a), stdexp::span_cast<std::byte, Scratchpad_Size>(scratchpad.data()));

        std::array<std::byte, 32> output{};
        blake2b::hash(output, stdexp::span_cast<std::byte>(rf), std::span<std::byte>{});

        return output;
    }

    RegisterFile Interpreter::execute(const RxProgram& program) {
        constexpr uint32_t Rx_Program_Iterations{ 2048 };

        ProgramContext ctx{ program };

        for (ctx.iter = 0; ctx.iter < Rx_Program_Iterations; ++ctx.iter) {
            initializeRegisters(ctx);

            for (ctx.ic = 0; ctx.ic < program.instructions.size(); ++ctx.ic) {
                const auto& instr = program.instructions[ctx.ic];
                if (ctx.iter < 3) {
                    slog("pre ic", ctx.ic, "checksum", checksum(&ctx.rf));
                }
                executeInstruction(ctx, instr);
            }

            slog("after iter", ctx.iter, "checksum", checksum(&ctx.rf));
            finalizeRegisters(ctx);
        }

        return ctx.rf;
    }

    void Interpreter::initializeRegisters(ProgramContext& ctx) {
        const uint64_t spMix{ ctx.rf.r[ctx.cfg.read_reg[0]] ^ ctx.rf.r[ctx.cfg.read_reg[1]] };
        ctx.sp_addr.mx ^= spMix;
        ctx.sp_addr.mx &= ScratchpadL3Mask64;
        ctx.sp_addr.ma ^= spMix >> 32;
        ctx.sp_addr.ma &= ScratchpadL3Mask64;

        for (uint32_t i = 0; i < Int_Register_Count; ++i) {
            const auto offset{ ctx.sp_addr.mx + 8 * i };
            ctx.rf.r[i] ^= scratchpad.read(offset);
        }

        for (uint32_t i = 0; i < Float_Register_Count; ++i) {
            const auto offset{ ctx.sp_addr.ma + 8 * i };
            ctx.rf.f[i] = intrinsics::sse::cvtpi32<double>(scratchpad.data() + offset);
        }

        for (uint32_t i = 0; i < Float_Register_Count; ++i) {
            const auto offset{ ctx.sp_addr.ma + 8 * (4 + i) };
            const auto x{ intrinsics::sse::cvtpi32<double>(scratchpad.data() + offset) };

            ctx.rf.e[i] = maskRegisterExponentMantissa(x, ctx.cfg);
        }
    }

    void Interpreter::executeInstruction(ProgramContext& ctx, const RxInstruction& instr) {
        const auto src_register{ instr.src % Int_Register_Count };
        const auto dst_register{ instr.dst % Int_Register_Count };
        const auto f_src_register{ instr.src % Float_Register_Count };
        const auto f_dst_register{ instr.dst % Float_Register_Count };

        const auto bc{ static_cast<uint8_t>(LUT_Opcode[instr.opcode]) };
        const bool is_integer_instr{ bc < static_cast<uint8_t>(Bytecode::FSWAP_R) || bc > static_cast<uint8_t>(Bytecode::FSQRT_R) };

        const auto offset = [&]() {
            const auto imm{ signExtend2sCompl(instr.imm32) };

            // this condition works only for integer instructions
            if (is_integer_instr && src_register == dst_register) {
                return imm & ScratchpadL3Mask;
            }

            const auto mem_mask{ instr.modMask() ? ScratchpadL1Mask : ScratchpadL2Mask };
            const auto src_value{ ctx.rf.r[src_register] };

            return (src_value + imm) & mem_mask;
        }();

        auto r_src_value{ ctx.rf.r[src_register] };
        auto m_src_value{ scratchpad.read(offset) }; 
        const auto mem_src_value{ intrinsics::sse::cvtpi32<double>(scratchpad.data() + offset) };


        switch (LUT_Opcode[instr.opcode]) { using enum Bytecode;
        case IADD_RS:
        {
            auto imm = (dst_register != 5) ? 0 : signExtend2sCompl(instr.imm32);
            ctx.rf.r[dst_register] += (r_src_value << instr.modShift()) + imm;
            break;
        }
        case IADD_M:
            ctx.rf.r[dst_register] += m_src_value;
            break;
        case ISUB_R:
            if (src_register == dst_register) {
                r_src_value = signExtend2sCompl(instr.imm32);
            }

            ctx.rf.r[dst_register] -= r_src_value;
            break;
        case ISUB_M:
            ctx.rf.r[dst_register] -= m_src_value;
            break;
        case IMUL_R:
            if (src_register == dst_register) {
                r_src_value = signExtend2sCompl(instr.imm32);
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
            ctx.rf.r[dst_register] = intrinsics::smulh(unsigned64ToSigned2sCompl(ctx.rf.r[dst_register]), unsigned64ToSigned2sCompl(r_src_value));
            break;
        case ISMULH_M:
            ctx.rf.r[dst_register] = intrinsics::smulh(unsigned64ToSigned2sCompl(ctx.rf.r[dst_register]), unsigned64ToSigned2sCompl(m_src_value));
            break;
        case IMUL_RCP:
        {
            const uint64_t divisor{ instr.imm32 };
            if (!std::has_single_bit(divisor)) {
                r_src_value = reciprocal(divisor);
                ctx.rf.r[dst_register] *= r_src_value;
            }
            break;
        }
        case INEG_R:
            ctx.rf.r[dst_register] = ~(ctx.rf.r[dst_register]) + 1; //two's complement negative
            break;
        case IXOR_R:
            if (src_register == dst_register) {
                r_src_value = signExtend2sCompl(instr.imm32);
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
                ctx.rf.f[f_dst_register] = intrinsics::sse::swap(ctx.rf.f[f_dst_register]);
                break;
            }
                
            ctx.rf.e[f_dst_register] = intrinsics::sse::swap(ctx.rf.e[f_dst_register]);
            break;
        case FADD_R:
            ctx.rf.f[f_dst_register] = intrinsics::sse::add(ctx.rf.f[f_dst_register], ctx.rf.a[f_src_register]);
            break;
        case FADD_M:
            ctx.rf.f[f_dst_register] = intrinsics::sse::add(ctx.rf.f[f_dst_register], mem_src_value);
            break;
        case FSUB_R:
            ctx.rf.f[f_dst_register] = intrinsics::sse::sub(ctx.rf.f[f_dst_register], ctx.rf.a[f_src_register]);
            break;
        case FSUB_M:
            ctx.rf.f[f_dst_register] = intrinsics::sse::sub(ctx.rf.f[f_dst_register], mem_src_value);
            break;
        case FSCAL_R:
        {
            const auto mask{ intrinsics::sse::bcasti64<double>(0x80F0000000000000) };
            ctx.rf.f[f_dst_register] = intrinsics::sse::vxor(ctx.rf.f[f_dst_register], mask);
            break;
        }
        case FMUL_R:
            ctx.rf.e[f_dst_register] = intrinsics::sse::mul(ctx.rf.e[f_dst_register], ctx.rf.a[f_src_register]);
            break;
        case FDIV_M:
        {
            const auto f_src_value{ maskRegisterExponentMantissa(mem_src_value, ctx.cfg) };
            ctx.rf.e[f_dst_register] = intrinsics::sse::div(ctx.rf.e[f_dst_register], f_src_value);
            break;
        }
        case FSQRT_R:
            ctx.rf.e[f_dst_register] = intrinsics::sse::sqrt(ctx.rf.e[f_dst_register]);
            break;
        case CBRANCH:
        {
            constexpr uint32_t Condition_Offset{ 8 };
            constexpr uint32_t Condition_Mask{ 255 };

            const auto shift{ instr.modCond() + Condition_Offset };
            const auto mem_mask{ Condition_Mask << shift }; 
            
            auto imm = signExtend2sCompl(instr.imm32) | (1ULL << shift);
            if (Condition_Offset > 0 || shift > 0) { // Clear the bit belwo the condition mask - this limits the number of successive jumps to 2
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
            intrinsics::sse::setFloatRoundingMode(std::rotr(r_src_value, imm) % 4);
            break;
        }
        case ISTORE:
        {
            const auto imm{ signExtend2sCompl(instr.imm32) };
            constexpr uint32_t StoreL3Condition = 14;
            auto mem_mask{ ScratchpadL3Mask };
            if (instr.modCond() < StoreL3Condition)
                mem_mask = instr.modMask() ? ScratchpadL1Mask : ScratchpadL2Mask;

            const auto store_offset{ (ctx.rf.r[dst_register] + imm) & mem_mask };
            scratchpad.write(store_offset, r_src_value);
            break;
        }
        default:
            std::unreachable();
        }
    }

    void Interpreter::finalizeRegisters(ProgramContext& ctx) {
        ctx.mem.mx ^= ctx.rf.r[ctx.cfg.read_reg[2]] ^ ctx.rf.r[ctx.cfg.read_reg[3]];
        ctx.mem.mx &= Cache_Line_Align_Mask;

        const auto dt_index{ (ctx.cfg.dataset_offset + ctx.mem.ma) / Cache_Line_Size };
        DatasetItem dt_item{ dataset[dt_index] };
        std::swap(ctx.mem.mx, ctx.mem.ma);

        for (uint32_t i = 0; i < Int_Register_Count; ++i) {
            ctx.rf.r[i] ^= dt_item[i];
            const auto offset{ ctx.sp_addr.ma + 8 * i };
            scratchpad.write(offset, ctx.rf.r[i]);
        }

        slog("checksum after dataset item", checksum(&ctx.rf));

        for (uint32_t i = 0; i < Float_Register_Count; ++i) {
            ctx.rf.f[i] = intrinsics::sse::vxor(ctx.rf.f[i], ctx.rf.e[i]);
        }

        for (uint32_t i = 0; i < Float_Register_Count; ++i) {
            const auto offset{ ctx.sp_addr.mx + 16 * i };
            scratchpad.write(offset, std::bit_cast<uint64_t>(ctx.rf.f[i][0]));
            scratchpad.write(offset + 8, std::bit_cast<uint64_t>(ctx.rf.f[i][1]));
        }

        ctx.sp_addr.mx = 0;
        ctx.sp_addr.ma = 0;
    }

    ProgramContext::ProgramContext(const RxProgram& program) {
        const auto entropy{ program.entropy };

        constexpr uint64_t Dataset_Extra_Items{ 524287 };

        rf.a[0][0] = std::bit_cast<double>(getSmallPositiveFloatBits(entropy[0]));
        rf.a[0][1] = std::bit_cast<double>(getSmallPositiveFloatBits(entropy[1]));
        rf.a[1][0] = std::bit_cast<double>(getSmallPositiveFloatBits(entropy[2]));
        rf.a[1][1] = std::bit_cast<double>(getSmallPositiveFloatBits(entropy[3]));
        rf.a[2][0] = std::bit_cast<double>(getSmallPositiveFloatBits(entropy[4]));
        rf.a[2][1] = std::bit_cast<double>(getSmallPositiveFloatBits(entropy[5]));
        rf.a[3][0] = std::bit_cast<double>(getSmallPositiveFloatBits(entropy[6]));
        rf.a[3][1] = std::bit_cast<double>(getSmallPositiveFloatBits(entropy[7]));

        mem.ma = static_cast<uint32_t>(entropy[8] & Cache_Line_Align_Mask);
        mem.mx = static_cast<uint32_t>(entropy[10]);

        sp_addr.ma = mem.ma;
        sp_addr.mx = mem.mx;

        for (uint32_t i = 0; i < cfg.read_reg.size(); ++i) {
            cfg.read_reg[i] = (i * 2) + ((entropy[12] >> i) & 1);
        }

        cfg.dataset_offset = (entropy[13] % (Dataset_Extra_Items + 1)) * Cache_Line_Size;

        cfg.e_mask[0] = floatMask(entropy[14]);
        cfg.e_mask[1] = floatMask(entropy[15]);

        reg_usage.fill(-1);
        
        const auto setRegUsage = [this](const uint32_t dst, const uint32_t src, const uint32_t ic, const bool with_src = false, const bool all = false) -> void {
            if (all) {
                for (int i = 0; i < reg_usage.size(); i++) {
                    reg_usage[i] = ic;
                }
                return;
            }

            reg_usage[dst] = ic;
            if (with_src) reg_usage[src] = ic;
        };

        for (uint32_t i = 0; i < program.instructions.size(); ++i) {
            const auto dst_register{ program.instructions[i].dst % Int_Register_Count };
            const auto src_register{ program.instructions[i].src % Int_Register_Count };

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
                setRegUsage(dst_register, src_register, i);
                break;
            case IMUL_RCP:
                if (!std::has_single_bit(program.instructions[i].imm32)) {
                    setRegUsage(dst_register, src_register, i);
                }
                break;
            case ISWAP_R:
                if (src_register != dst_register) {
                    setRegUsage(dst_register, src_register, i, true);
                }
                break;
            case CBRANCH:
                branch_target[i] = reg_usage[dst_register];
                setRegUsage(dst_register, src_register, i, true, true);
                break;
            default:
                break;
            }
        }
    }

    namespace {
        constexpr uint64_t getSmallPositiveFloatBits(const uint64_t entropy) {
            constexpr uint64_t exponentSize{ 11 };
            constexpr uint64_t mantissaMask{ (1ULL << Mantissa_Size) - 1 };
            constexpr uint64_t exponentMask{ (1ULL << exponentSize) - 1 };
            constexpr uint64_t exponentBias{ 1023 };

            const auto mantissa{ entropy & mantissaMask };
            auto exponent{ entropy >> 59 }; //0..31

            exponent += exponentBias;
            exponent &= exponentMask;
            exponent <<= Mantissa_Size;
            return exponent | mantissa;
        }

        constexpr uint64_t getStaticExponent(const uint64_t entropy) {
            constexpr uint64_t staticExponentBits{ 4 };
            constexpr uint64_t constExponentBits{ 0x300 };

            auto exponent{ constExponentBits };
            exponent |= (entropy >> (64 - staticExponentBits)) << Dynamic_Exponent_Bits;
            exponent <<= Mantissa_Size;
            return exponent;
        }

        constexpr uint64_t floatMask(const uint64_t entropy) {
            constexpr uint64_t mask22bit{ (1ULL << 22) - 1 };
            return (entropy & mask22bit) | getStaticExponent(entropy);
        }

        constexpr uint64_t signExtend2sCompl(const uint32_t x) {
            return (-1 == ~0) ? (int64_t)(int32_t)(x) : (x > INT32_MAX ? (x | 0xffffffff00000000ULL) : (uint64_t)x);
        }

        constexpr int64_t unsigned64ToSigned2sCompl(const uint64_t x) {
            return (-1 == ~0) ? (int64_t)x : (x > INT64_MAX ? (-(int64_t)(UINT64_MAX - x) - 1) : (int64_t)x);
        }

        F128Reg maskRegisterExponentMantissa(F128Reg x, const ProgramConfiguration& config) {
            const F128Reg xmantissaMask{
                std::bit_cast<double>(Dynamic_Mantissa_Mask),
                std::bit_cast<double>(Dynamic_Mantissa_Mask),
            };

            const F128Reg xexponentMask{
                std::bit_cast<double>(config.e_mask[0]),
                std::bit_cast<double>(config.e_mask[1]),
            };

            x = intrinsics::sse::vand(x, xmantissaMask);
            x = intrinsics::sse::vor(x, xexponentMask);

            return x;
        }
    }
}

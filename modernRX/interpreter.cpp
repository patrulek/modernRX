#include <algorithm>
#include <array>
#include <utility>

#include "aes1rhash.hpp"
#include "aes4rrandom.hpp"
#include "assembler.hpp"
#include "bytecode.hpp"
#include "interpreter.hpp"
#include "intrinsics.hpp"
#include "randomxparams.hpp"
#include "reciprocal.hpp"
#include "virtualmem.hpp"

#include <print>

namespace modernRX {
    namespace {
        constexpr uint32_t Int_Register_Count{ 8 };
        constexpr uint32_t Float_Register_Count{ 4 };
        constexpr uint32_t Scratchpad_L1_Mask{ (Rx_Scratchpad_L1_Size - 1) & ~7 }; // L1 cache 8-byte alignment mask.
        constexpr uint32_t Scratchpad_L2_Mask{ (Rx_Scratchpad_L2_Size - 1) & ~7 }; // L2 cache 8-byte alignment mask.
        constexpr uint32_t Scratchpad_L3_Mask{ (Rx_Scratchpad_L3_Size - 1) & ~7 }; // L3 cache 8-byte alignment mask.
        constexpr uint32_t Scratchpad_L3_Mask64{ (Rx_Scratchpad_L3_Size - 1) & ~63 }; // L3 cache 64-byte alignment mask.

        constexpr uint64_t Mantissa_Size{ 52 };
        constexpr uint64_t Cache_Line_Size{ sizeof(DatasetItem) };
        constexpr uint64_t Cache_Line_Align_Mask{ (Rx_Dataset_Base_Size - 1) & ~(Cache_Line_Size - 1) }; // Dataset 64-byte alignment mask.

        // Holds representation of register file used by interpreter during program execution.
        // https://github.com/tevador/RandomX/blob/master/doc/specs.md#43-registers
        struct RegisterFile {
            std::array<uint64_t, Int_Register_Count> r{}; // Common integer registers. Source or destination of integer instructions. Can be used as address registers for scratchpad access.
            std::array<intrinsics::xmm128d_t, Float_Register_Count> f{}; // "Additive" registers. Destination of floating point addition and substraction instructions.
            std::array<intrinsics::xmm128d_t, Float_Register_Count> e{}; // "Multiplicative" registers. Destination of floating point multiplication, division and square root instructions.
            std::array<intrinsics::xmm128d_t, Float_Register_Count> a{}; // Read-only, fixed-value floating point registers. Source operand of any floating point instruction.
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
        [[nodiscard]] intrinsics::xmm128d_t convertFloatRegister(const intrinsics::xmm128d_t x, const_span<uint64_t, 2> mask) noexcept;
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
    static_assert(sizeof(RxProgram) == Rx_Program_Bytes_Size); // Size of random program is also used in different context. Make sure both values match.

    // Holds RandomX program context: registers, memory addresses, configuration, etc.
    struct ProgramContext {
        // Initializes program context with given program.
        [[nodiscard]] explicit ProgramContext(const RxProgram& program);

        RegisterFile rf{};
        ProgramConfiguration cfg{};
        MemoryRegisters mem{};
        MemoryRegisters sp_addr{};
        uint32_t iter{ 0 };
    };

    Interpreter::Interpreter(std::span<std::byte, 64> seed, const_span<DatasetItem> dataset)
        : dataset(dataset), scratchpad(seed) {
        std::memcpy(this->seed.data(), seed.data(), seed.size());
    }

    std::array<std::byte, 32> Interpreter::execute() {
        // RandomX requires specific float environment before executing any program.
        // This RAII object will set proper float flags on creation and restore its values on destruction. 
        const intrinsics::sse::FloatEnvironment fenv{};

        for (uint32_t i = 0; i < Rx_Program_Count - 1; ++i) {
            auto [ctx, program] { generateProgram() };
            compileProgram(program);
            executeProgram(ctx, program);
            blake2b::hash(seed, span_cast<std::byte, sizeof(ctx.rf)>(ctx.rf));
        }

        auto [ctx, program] { generateProgram() };
        compileProgram(program);
        executeProgram(ctx, program);
        aes::hash1R(span_cast<std::byte, sizeof(ctx.rf.a)>(ctx.rf.a), span_cast<std::byte, Rx_Scratchpad_L3_Size>(scratchpad.data()));
        std::array<std::byte, 32> output{};
        blake2b::hash(output, span_cast<std::byte>(ctx.rf));

        return output;
    }

    std::pair<ProgramContext, RxProgram> Interpreter::generateProgram() {
        RxProgram program{};
        aes::fill4R(span_cast<std::byte>(program), seed);

        // Last 64 bytes of the program are now the new seed.

        return std::make_pair(ProgramContext{ program }, program);
    }

    void Interpreter::executeProgram(ProgramContext& ctx, const RxProgram& program) {
        intrinsics::prefetch<intrinsics::PrefetchMode::NTA, void>(dataset[(ctx.cfg.dataset_offset + ctx.mem.ma) / Cache_Line_Size].data());

        // https://github.com/tevador/RandomX/blob/master/doc/specs.md#462-loop-execution
        for (ctx.iter = 0; ctx.iter < Rx_Program_Iterations; ++ctx.iter) {
            // Steps 2-3.
            initializeRegisters(ctx);

            // Step 4.
            (*jit)(reinterpret_cast<uintptr_t>(&ctx.rf), reinterpret_cast<uintptr_t>(&ctx.cfg.e_mask), reinterpret_cast<uintptr_t>(scratchpad.data()));

            // Steps 1 and 5-12.
            finalizeRegisters(ctx);

            // Step 13. - increase iteration counter.
        }
    }

    void Interpreter::initializeRegisters(ProgramContext& ctx) {
        // Step 2.
        for (uint32_t i = 0; i < Int_Register_Count; ++i) {
            const auto offset{ ctx.sp_addr.mx + 8 * i };
            ctx.rf.r[i] ^= scratchpad.read<uint64_t>(offset);
        }

        // Step 3.
        // -----
        // "F-group" register initialization: https://github.com/tevador/RandomX/blob/master/doc/specs.md#431-group-f-register-conversion
        for (uint32_t i = 0; i < Float_Register_Count; ++i) {
            const auto offset{ ctx.sp_addr.ma + 8 * i };
            ctx.rf.f[i] = scratchpad.read<intrinsics::xmm128d_t>(offset);
        }

        // "E-group" register initialization: https://github.com/tevador/RandomX/blob/master/doc/specs.md#432-group-e-register-conversion
        for (uint32_t i = 0; i < Float_Register_Count; ++i) {
            const auto offset{ ctx.sp_addr.ma + 8 * (4 + i) };
            const auto x{ scratchpad.read<intrinsics::xmm128d_t>(offset) };

            ctx.rf.e[i] = convertFloatRegister(x, ctx.cfg.e_mask);
        }
    }

    void Interpreter::finalizeRegisters(ProgramContext& ctx) {
        // Step 5.
        ctx.mem.mx ^= ctx.rf.r[ctx.cfg.read_reg[2]] ^ ctx.rf.r[ctx.cfg.read_reg[3]];
        ctx.mem.mx &= Cache_Line_Align_Mask;

        // Step 6.
        intrinsics::prefetch<intrinsics::PrefetchMode::NTA, void>(dataset[(ctx.cfg.dataset_offset + ctx.mem.mx) / Cache_Line_Size].data());

        // Step 7
        const auto dt_index{ (ctx.cfg.dataset_offset + ctx.mem.ma) / Cache_Line_Size };
        DatasetItem dt_item{ dataset[dt_index] };

        // Step 8.
        std::swap(ctx.mem.mx, ctx.mem.ma);

        // Step 9.
        for (uint32_t i = 0; i < Int_Register_Count; ++i) {
            ctx.rf.r[i] ^= dt_item[i];
        }

        scratchpad.write(ctx.sp_addr.ma, &ctx.rf.r, sizeof(ctx.rf.r));

        // Step 10.
        for (uint32_t i = 0; i < Float_Register_Count; ++i) {
            ctx.rf.f[i] = intrinsics::sse::vxor<double>(ctx.rf.f[i], ctx.rf.e[i]);
        }

        // Step 11.
        scratchpad.write(ctx.sp_addr.mx, &ctx.rf.f, sizeof(ctx.rf.f));

        // Step 12. - omitted.
        // Step 1.
        const uint64_t spMix{ ctx.rf.r[ctx.cfg.read_reg[0]] ^ ctx.rf.r[ctx.cfg.read_reg[1]] };
        ctx.sp_addr.mx = static_cast<uint32_t>(spMix) & Scratchpad_L3_Mask64;
        ctx.sp_addr.ma = static_cast<uint32_t>(spMix >> 32) & Scratchpad_L3_Mask64;
    }


    ProgramContext::ProgramContext(const RxProgram& program) {
        constexpr uint64_t Dataset_Extra_Items{ Rx_Dataset_Extra_Size / sizeof(DatasetItem) };
        const auto& entropy{ program.entropy };
        
        // "A-group" register initialization: https://github.com/tevador/RandomX/blob/master/doc/specs.md#452-group-a-register-initialization
        rf.a[0] = { getSmallPositiveFloat(entropy[0]), getSmallPositiveFloat(entropy[1]) }; 
        rf.a[1] = { getSmallPositiveFloat(entropy[2]), getSmallPositiveFloat(entropy[3]) };
        rf.a[2] = { getSmallPositiveFloat(entropy[4]), getSmallPositiveFloat(entropy[5]) };
        rf.a[3] = { getSmallPositiveFloat(entropy[6]), getSmallPositiveFloat(entropy[7]) };

        // Memory registers initialization: https://github.com/tevador/RandomX/blob/master/doc/specs.md#453-memory-registers
        mem.ma = static_cast<uint32_t>(entropy[8] & Cache_Line_Align_Mask);
        mem.mx = static_cast<uint32_t>(entropy[10]);

        // Address registers initialization: https://github.com/tevador/RandomX/blob/master/doc/specs.md#454-address-registers
        for (uint32_t i = 0; i < cfg.read_reg.size(); ++i) {
            cfg.read_reg[i] = (i * 2) + ((entropy[12] >> i) & 1);
        }

        // Dataset offset initialization: https://github.com/tevador/RandomX/blob/master/doc/specs.md#455-dataset-offset
        cfg.dataset_offset = (entropy[13] % (Dataset_Extra_Items + 1)) * Cache_Line_Size;

        // "E-group" register masks initialization: https://github.com/tevador/RandomX/blob/master/doc/specs.md#456-group-e-register-masks
        cfg.e_mask[0] = getFloatRegisterMask(entropy[14]);
        cfg.e_mask[1] = getFloatRegisterMask(entropy[15]);

        // Scratchpad address registers initialization: https://github.com/tevador/RandomX/blob/master/doc/specs.md#461-initialization
        const uint64_t spMix{ rf.r[cfg.read_reg[0]] ^ rf.r[cfg.read_reg[1]] };
        sp_addr.ma = (mem.ma ^ static_cast<uint32_t>(spMix)) & Scratchpad_L3_Mask64;
        sp_addr.mx = (mem.mx ^ static_cast<uint32_t>(spMix >> 32)) & Scratchpad_L3_Mask64;
    }

    void Interpreter::compileProgram(const RxProgram& program) {
        using namespace assembler;
        using namespace assembler::registers;

        Context asmb(16 * 1024);

        asmb.push(RBP, RSP, RBX, RSI, RDI, R12, R13, R14, R15, XMM6, XMM7, XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15);

        // Scratchpad address passed in R8 register, move to RSI.
        asmb.mov(RSI, R08);

        // Regfile address passed in RCX register, move to RBX.
        asmb.mov(RBX, RCX);

        // e_mask address passed in RDX registerm move to RDI.
        asmb.mov(RDI, RDX);

        // Set Mantissa_Mask in XMM15 register.
        constexpr uint64_t Mantissa_Mask{ 0x00FF'FFFF'FFFF'FFFF };
        const auto mantissa_mask_reg{ XMM15 };
        asmb.vpbroadcastq(mantissa_mask_reg, Mantissa_Mask);

        // Set Scale_Mask in XMM14 register.
        constexpr uint64_t Scale_Mask{ 0x80F0'0000'0000'0000 };
        const auto scale_mask_reg{ XMM14 };
        asmb.vpbroadcastq(scale_mask_reg, Scale_Mask);

        std::array<int32_t, Int_Register_Count> reg_usage{};
        reg_usage.fill(-1);

        // Load all registers from ctx object.
        for (int i = 0; i < 8; ++i) {
            asmb.mov(Register::GPR(8 + i), RCX[i * 8]);

            if (i < 4) {
                asmb.vmovdqu(Register::XMM(i), RCX[64 + i * 16]); // 'f' registers.
                asmb.vmovdqu(Register::XMM(i + 4), RCX[128 + i * 16]); // 'e' registers.
                asmb.vmovdqu(Register::XMM(i + 8), RCX[192 + i * 16]); // 'a' registers.
            }
        }

        const auto tmp_xmm_reg{ XMM12 };
        const auto tmp_xmm_reg2{ XMM13 };

        // Compile all instructions.
        for (uint32_t i = 0; i < program.instructions.size(); ++i) {
            // Save current program counter to make it jumpable to any instruction.
            asmb.label(std::to_string(i), false);

            // Getting registers indexes. Modulo is used to handle register overflow.
            const RxInstruction& instr{ program.instructions[i] };

            const auto dst_register{ instr.dst_register % Int_Register_Count };
            const auto src_register{ instr.src_register % Int_Register_Count };
            const auto f_dst_register{ dst_register % Float_Register_Count };
            const auto f_src_register{ src_register % Float_Register_Count };

            // r8-r15: integer registers.
            const auto asmb_dst_reg{ Register::GPR(dst_register | 8) };
            const auto asmb_src_reg{ Register::GPR(src_register | 8) };

            // xmm0-xmm3: float 'f' registers.
            const auto f_asmb_dst_reg{ Register::XMM(f_dst_register) };
            const auto f_asmb_src_reg{ Register::XMM(f_src_register) };

            // xmm4-xmm7: float 'e' registers.
            const auto e_asmb_dst_reg{ Register::XMM(f_dst_register + 4) };
            const auto e_asmb_src_reg{ Register::XMM(f_src_register + 4) };

            // xmm8-xmm11: float 'a' registers.
            const auto a_asmb_dst_reg{ Register::XMM(f_dst_register | 8) };
            const auto a_asmb_src_reg{ Register::XMM(f_src_register | 8) };

            // Scratchpad memory mask.
            auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };
                                    
            switch (LUT_Opcode[instr.opcode]) { using enum Bytecode;
            case IADD_RS:
            {
                constexpr uint8_t Displacement_Reg_Idx{ 5 };
                const bool with_imm{ dst_register == Displacement_Reg_Idx };
                const auto offset = with_imm ? instr.imm32 : 0;

                reg_usage[dst_register] = i;
                asmb.lea(asmb_dst_reg, asmb_dst_reg[asmb_src_reg[offset]], 1 << instr.modShift());
                break;
            }
            case IADD_M:
            {
                reg_usage[dst_register] = i;

                if (dst_register == src_register) {
                    const auto offset{ instr.imm32 & Scratchpad_L3_Mask };
                    asmb.add(asmb_dst_reg, RSI[offset]);
                } else {
                    asmb.lea(RAX, asmb_src_reg[instr.imm32]);
                    asmb.and_(RAX, mem_mask);
                    asmb.add(asmb_dst_reg, RSI[RAX[0]]);
                }

                break;
            }
            case ISUB_R:
            {
                reg_usage[dst_register] = i;

                if (dst_register == src_register) {
                    asmb.sub(asmb_dst_reg, static_cast<int32_t>(instr.imm32));
                } else {
                    asmb.sub(asmb_dst_reg, asmb_src_reg);
                }

                break;
            }
            case ISUB_M:
            {
                reg_usage[dst_register] = i;

                if (dst_register == src_register) {
                    const auto offset{ instr.imm32 & Scratchpad_L3_Mask };
                    asmb.sub(asmb_dst_reg, RSI[offset]);
                } else {
                    asmb.lea(RAX, asmb_src_reg[instr.imm32]);
                    asmb.and_(RAX, mem_mask);
                    asmb.sub(asmb_dst_reg, RSI[RAX[0]]);
                }

                break;
            }
            case IMUL_R:
            {
                reg_usage[dst_register] = i;

                if (dst_register == src_register) {
                    asmb.imul(asmb_dst_reg, static_cast<int32_t>(instr.imm32));
                } else {
                    asmb.imul(asmb_dst_reg, asmb_src_reg);
                }

                break;
            }
            case IMUL_M:
            {
                reg_usage[dst_register] = i;

                if (dst_register == src_register) {
                    const auto offset{ instr.imm32 & Scratchpad_L3_Mask };
                    asmb.imul(asmb_dst_reg, RSI[offset]);
                } else {
                    asmb.lea(RCX, asmb_src_reg[instr.imm32]);
                    asmb.and_(RCX, mem_mask);
                    asmb.imul(asmb_dst_reg, RSI[RCX[0]]);
                }

                break;
            }
            case IMULH_R:
            {
                reg_usage[dst_register] = i;
                asmb.mulh(asmb_dst_reg, asmb_src_reg);

                break;
            }
            case IMULH_M:
            {
                reg_usage[dst_register] = i;

                if (dst_register == src_register) {
                    const auto offset{ instr.imm32 & Scratchpad_L3_Mask };
                    asmb.mulh(asmb_dst_reg, RSI[offset]);
                } else {
                    asmb.lea(RCX, asmb_src_reg[instr.imm32]);
                    asmb.and_(RCX, mem_mask);
                    asmb.mulh(asmb_dst_reg, RSI[RCX[0]]);
                }

                break;
            }
            case ISMULH_R:
            {
                reg_usage[dst_register] = i;
                asmb.imulh(asmb_dst_reg, asmb_src_reg);

                break;
            }
            case ISMULH_M:
            {
                reg_usage[dst_register] = i;

                if (dst_register == src_register) {
                    const auto offset{ instr.imm32 & Scratchpad_L3_Mask };
                    asmb.imulh(asmb_dst_reg, RSI[offset]);
                } else {
                    asmb.lea(RCX, asmb_src_reg[instr.imm32]);
                    asmb.and_(RCX, mem_mask);
                    asmb.imulh(asmb_dst_reg, RSI[RCX[0]]);
                }

                break;
            }
            case INEG_R:
            {
                reg_usage[dst_register] = i;
                asmb.neg(asmb_dst_reg);

                break;
            }
            case IXOR_R:
            {
                reg_usage[dst_register] = i;

                if (dst_register == src_register) {
                    asmb.xor_(asmb_dst_reg, static_cast<int32_t>(instr.imm32));
                } else {
                    asmb.xor_(asmb_dst_reg, asmb_src_reg);
                }

                break;
            }
            case IXOR_M:
            {
                reg_usage[dst_register] = i;

                if (dst_register == src_register) {
                    const auto offset{ instr.imm32 & Scratchpad_L3_Mask };
                    asmb.xor_(asmb_dst_reg, RSI[offset]);
                } else {
                    asmb.lea(RAX, asmb_src_reg[instr.imm32]);
                    asmb.and_(RAX, mem_mask);
                    asmb.xor_(asmb_dst_reg, RSI[RAX[0]]);
                }

                break;
            }
            case IROR_R:
            {
                reg_usage[dst_register] = i;

                if (dst_register == src_register) {
                    asmb.ror(asmb_dst_reg, instr.imm32 % 64);    
                } else {
                    asmb.ror(asmb_dst_reg, asmb_src_reg);
                }

                break;
            }
            case IROL_R:
            {
                reg_usage[dst_register] = i;
                
                if (dst_register == src_register) {
                    asmb.rol(asmb_dst_reg, instr.imm32 % 64);
                } else {
                    asmb.rol(asmb_dst_reg, asmb_src_reg);
                }

                break;
            }
            case IMUL_RCP:
            {
                if (instr.imm32 != 0 && !std::has_single_bit(instr.imm32)) {
                    reg_usage[dst_register] = i;
                    asmb.mov(RAX, reciprocal(instr.imm32));
                    asmb.imul(asmb_dst_reg, RAX);
                }

                break;
            }
            case ISWAP_R:
                if (src_register != dst_register) {
                    reg_usage[dst_register] = i;
                    reg_usage[src_register] = i;
                    asmb.xchg(asmb_dst_reg, asmb_src_reg);
                }

                break;
            case CBRANCH:
            {
                constexpr uint32_t Condition_Mask{ (1 << Rx_Jump_Bits) - 1 };
                const auto shift{ instr.modCond() + Rx_Jump_Offset };
                const auto mem_mask{ Condition_Mask << shift };

                static_assert(Rx_Jump_Offset > 0, "Below simplification requires this assertion");
                uint64_t imm{ static_cast<int32_t>(instr.imm32) | (1ULL << shift) };
                imm &= ~(1ULL << (shift - 1)); // Clear the bit below the condition mask - this limits the number of successive jumps to 2.

                asmb.add(asmb_dst_reg, static_cast<int32_t>(imm));
                asmb.test(asmb_dst_reg, mem_mask);
                asmb.jz(std::to_string(reg_usage[dst_register] + 1));
                
                reg_usage.fill(i); // Set all registers as used.

                break;
            }
            case FSWAP_R:
            {
                const auto& swapreg{ dst_register < Float_Register_Count ? f_asmb_dst_reg : e_asmb_dst_reg };
                asmb.vshufpd(swapreg, swapreg, swapreg, 0b0000'0001);
                break;
            }
            case FADD_R:
            {
                asmb.vaddpd(f_asmb_dst_reg, a_asmb_src_reg);
                break;
            }
            case FADD_M:
            {
                asmb.lea(RAX, asmb_src_reg[instr.imm32]);
                asmb.and_(RAX, mem_mask);
                asmb.vcvtdq2pd(tmp_xmm_reg, RSI[RAX[0]]);
                asmb.vaddpd(f_asmb_dst_reg, tmp_xmm_reg);
                
                break;
            }
            case FSUB_R:
            {
                asmb.vsubpd(f_asmb_dst_reg, a_asmb_src_reg);
                break;
            }
            case FSUB_M:
            {
                asmb.lea(RAX, asmb_src_reg[instr.imm32]);
                asmb.and_(RAX, mem_mask);
                asmb.vcvtdq2pd(tmp_xmm_reg, RSI[RAX[0]]);
                asmb.vsubpd(f_asmb_dst_reg, tmp_xmm_reg);

                break;
            }
            case FSCAL_R:
            {
                asmb.vpxor(f_asmb_dst_reg, f_asmb_dst_reg, scale_mask_reg);
                break;
            }
            case FMUL_R:
            {
                asmb.vmulpd(e_asmb_dst_reg, a_asmb_src_reg);
                break;
            }
            case FDIV_M:
            {
                asmb.vmovdqu(tmp_xmm_reg2, RDI[0]); // Load e_mask.
                asmb.lea(RAX, asmb_src_reg[instr.imm32]);
                asmb.and_(RAX, mem_mask);
                asmb.vcvtdq2pd(tmp_xmm_reg, RSI[RAX[0]]);
                asmb.vpand(tmp_xmm_reg, tmp_xmm_reg, mantissa_mask_reg);
                asmb.vpor(tmp_xmm_reg, tmp_xmm_reg, tmp_xmm_reg2);
                asmb.vdivpd(e_asmb_dst_reg, tmp_xmm_reg);

                break;
            }
            case FSQRT_R:
            {
                asmb.vsqrtpd(e_asmb_dst_reg);
                break;
            }
            case CFROUND:
            {
                asmb.mov(RAX, asmb_src_reg);
                asmb.ror(RAX, static_cast<int32_t>(instr.imm32 % 64));
                asmb.and_(RAX, 3);
                asmb.rol(RAX, 13);
                asmb.or_(RAX, intrinsics::sse::Rx_Mxcsr_Default);
                asmb.push(RAX);
                asmb.ldmxcsr(RSP[0]);
                asmb.pop(RAX);
                
                break;
            }
            case ISTORE:
            {
                constexpr uint32_t L3_Store_Condition{ 14 };

                if (instr.modCond() >= L3_Store_Condition) {
                    mem_mask = Scratchpad_L3_Mask;
                }

                asmb.lea(RAX, asmb_dst_reg[static_cast<int32_t>(instr.imm32)]);
                asmb.and_(RAX, mem_mask);
                asmb.mov(RSI[RAX[0]], asmb_src_reg);

                break;
            }
            default:
                std::unreachable();
            }
        }

        // Save registers in ctx object.
        for (int i = 0; i < 8; ++i) {
            asmb.mov(RBX[i * 8], Register::GPR(8 + i));

            if (i < 4) {
                asmb.vmovdqu(RBX[64 + i * 16], Register::XMM(i)); // 'f' registers.
                asmb.vmovdqu(RBX[128 + i * 16], Register::XMM(i + 4)); // 'e' registers.
                asmb.vmovdqu(RBX[192 + i * 16], Register::XMM(i + 8)); // 'a' registers.
            }
        }

        asmb.pop(XMM15, XMM14, XMM13, XMM12, XMM11, XMM10, XMM9, XMM8, XMM7, XMM6, R15, R14, R13, R12, RDI, RSI, RBX, RSP, RBP);
        asmb.ret();

        jit = makeExecutable<RxProgramJIT>(asmb.flushCode(), asmb.flushData());
    }

    namespace {
        // Used to initialize "A-group" register values:
        // https://github.com/tevador/RandomX/blob/master/doc/specs.md#452-group-a-register-initialization
        constexpr double getSmallPositiveFloat(const uint64_t entropy) noexcept {
            constexpr uint64_t Exponent_Size{ 11 };
            constexpr uint64_t Mantissa_Mask{ (1ULL << Mantissa_Size) - 1 };
            constexpr uint64_t Exponent_Mask{ (1ULL << Exponent_Size) - 1 };
            constexpr uint64_t Exponent_Bias{ 1023 };

            const auto mantissa{ entropy & Mantissa_Mask };

            auto exponent{ entropy >> 59 }; // 0 .. 31
            exponent += Exponent_Bias;
            exponent &= Exponent_Mask;
            exponent <<= Mantissa_Size;

            return std::bit_cast<double>(exponent | mantissa);
        }

        // Used to get "E-group" register masks:
        // https://github.com/tevador/RandomX/blob/master/doc/specs.md#456-group-e-register-masks
        constexpr uint64_t getFloatRegisterMask(const uint64_t entropy) noexcept {
            constexpr uint64_t Mask22_Bits{ (1ULL << 22) - 1 };
            constexpr uint64_t Exponent_Bits{ 4 };

            uint64_t exponent{ 0b01100000000 }; // Step 2. of: https://github.com/tevador/RandomX/blob/master/doc/specs.md#432-group-e-register-conversion
            exponent |= (entropy >> (64 - Exponent_Bits)) << Exponent_Bits;
            exponent <<= Mantissa_Size;

            return (entropy & Mask22_Bits) | exponent;
        }

        // Used to convert "E-group" registers values:
        // https://github.com/tevador/RandomX/blob/master/doc/specs.md#432-group-e-register-conversion
        intrinsics::xmm128d_t convertFloatRegister(const intrinsics::xmm128d_t x, const_span<uint64_t, 2> mask) noexcept {
            constexpr uint64_t Exponent_Bits{ 4 };
            constexpr uint64_t Mantissa_Mask{ (1ULL << (Mantissa_Size + Exponent_Bits)) - 1 };

            constexpr intrinsics::xmm128d_t Xmm_Mantissa_Mask{
                std::bit_cast<double>(Mantissa_Mask),
                std::bit_cast<double>(Mantissa_Mask),
            };

            const intrinsics::xmm128d_t xmm_exponent_mask{
                std::bit_cast<double>(mask[0]),
                std::bit_cast<double>(mask[1]),
            };

            const auto y{ intrinsics::sse::vand<double>(x, Xmm_Mantissa_Mask) };
            return intrinsics::sse::vor<double>(y, xmm_exponent_mask);
        }
    }
}

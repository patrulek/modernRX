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
        constexpr uint32_t Scratchpad_L1_Mask{ (Rx_Scratchpad_L1_Size - 1) & ~7 }; // L1 cache 8-byte alignment mask.
        constexpr uint32_t Scratchpad_L2_Mask{ (Rx_Scratchpad_L2_Size - 1) & ~7 }; // L2 cache 8-byte alignment mask.
        constexpr uint32_t Scratchpad_L3_Mask{ (Rx_Scratchpad_L3_Size - 1) & ~7 }; // L3 cache 8-byte alignment mask.

        constexpr uint64_t Mantissa_Size{ 52 };
        constexpr uint64_t Cache_Line_Size{ sizeof(DatasetItem) };
        constexpr uint64_t Cache_Line_Align_Mask{ (Rx_Dataset_Base_Size - 1) & ~(Cache_Line_Size - 1) }; // Dataset 64-byte alignment mask.

        using OpImpl = void(*)(ProgramContext&, const RxInstruction&, Scratchpad&);

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
        // Updates program's instructions src and dst registers.
        [[nodiscard]] explicit ProgramContext(RxProgram& program) noexcept;

        RegisterFile rf{};
        MemoryRegisters mem{};
        MemoryRegisters sp_addr{};
        ProgramConfiguration cfg{};
        std::array<int32_t, Int_Register_Count> reg_usage{};
        uint32_t ic{ 0 };
        uint32_t iter{ 0 };
        std::array<int16_t, Rx_Program_Size> branch_target{};
        std::array<OpImpl, Rx_Program_Size> op_impl{};
        std::array<uint64_t, Rx_Program_Size> rcp{};
        std::array<uint32_t, Rx_Program_Size> extra{};
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
            executeProgram(ctx, program);
            blake2b::hash(seed, span_cast<std::byte, sizeof(ctx.rf)>(ctx.rf));
        }

        auto [ctx, program] { generateProgram() };
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
            // This performs steps: 1-3
            initializeRegisters(ctx);

            // This loop performs step: 4
            for (ctx.ic = 0; ctx.ic < program.instructions.size(); ++ctx.ic) {
                const auto& instr{ program.instructions[ctx.ic] };
                ctx.op_impl[ctx.ic](ctx, instr, scratchpad);
            }

            // This performs steps: 5-12
            finalizeRegisters(ctx);
        }
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

    // Read scratchpad value.
    template <typename T>
    static T ReadScratchpadValue(const ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const auto imm{ static_cast<int32_t>(instr.imm32) }; // Two's complement sign extension.

        // L3 cache is used only for integer instructions.
        if constexpr (std::is_same_v<T, uint64_t>) {
            if (instr.src_register == instr.dst_register) {
                return scratchpad.read<T>(static_cast<uint64_t>(imm) & Scratchpad_L3_Mask);
            }
        }

        const auto mem_mask{ instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask };
        const auto src_value{ ctx.rf.r[instr.src_register] };

        return scratchpad.read<T>((src_value + imm) & mem_mask);
    };

    static void nopImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        // Do nothing.
    }

    static void callnextImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        ++ctx.ic;
        const auto& next_instr{ &instr + 1 };
        ctx.op_impl[ctx.ic](ctx, *next_instr, scratchpad);
    }

    // Without shift, without imm
    static void iaddrsImpl1(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const uint64_t r_src_value{ ctx.rf.r[instr.src_register] }; // Integer register source value.
        ctx.rf.r[instr.dst_register] += r_src_value;
    }

    // Wihout shift, with imm
    static void iaddrsImpl2(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const uint64_t r_src_value{ ctx.rf.r[instr.src_register] }; // Integer register source value.
        ctx.rf.r[instr.dst_register] += r_src_value + static_cast<int32_t>(instr.imm32);
    }

    // With shift, without imm
    static void iaddrsImpl3(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const uint64_t r_src_value{ ctx.rf.r[instr.src_register] }; // Integer register source value.
        ctx.rf.r[instr.dst_register] += (r_src_value << instr.modShift());
    }

    // With shift, with imm
    static void iaddrsImpl4(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const uint64_t r_src_value{ ctx.rf.r[instr.src_register] }; // Integer register source value.
        ctx.rf.r[instr.dst_register] += (r_src_value << instr.modShift()) + static_cast<int32_t>(instr.imm32);
    }

    static void iaddmImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        ctx.rf.r[instr.dst_register] += ReadScratchpadValue<uint64_t>(ctx, instr, scratchpad);
    } 

    // with imm
    static void isubrImpl1(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        ctx.rf.r[instr.dst_register] -= static_cast<int32_t>(instr.imm32);
    }

    // with src
    static void isubrImpl2(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const uint64_t r_src_value{ ctx.rf.r[instr.src_register] }; // Integer register source value.
        ctx.rf.r[instr.dst_register] -= r_src_value;
    }

    static void isubmImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        ctx.rf.r[instr.dst_register] -= ReadScratchpadValue<uint64_t>(ctx, instr, scratchpad);
    }

    // with imm
    static void imulrImpl1(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        ctx.rf.r[instr.dst_register] *= static_cast<int32_t>(instr.imm32);
    }

    // with src
    static void imulrImpl2(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const uint64_t r_src_value{ ctx.rf.r[instr.src_register] }; // Integer register source value.
        ctx.rf.r[instr.dst_register] *= r_src_value;
    }

    static void imulmImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        ctx.rf.r[instr.dst_register] *= ReadScratchpadValue<uint64_t>(ctx, instr, scratchpad);
    }

    static void imulhrImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const uint64_t r_src_value{ ctx.rf.r[instr.src_register] }; // Integer register source value.
        ctx.rf.r[instr.dst_register] = intrinsics::umulh(ctx.rf.r[instr.dst_register], r_src_value);
    }

    static void imulhmImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        ctx.rf.r[instr.dst_register] = intrinsics::umulh(ctx.rf.r[instr.dst_register], ReadScratchpadValue<uint64_t>(ctx, instr, scratchpad));
    }

    static void ismulhrImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const uint64_t r_src_value{ ctx.rf.r[instr.src_register] }; // Integer register source value.
        ctx.rf.r[instr.dst_register] = intrinsics::smulh(ctx.rf.r[instr.dst_register], r_src_value);
    }

    static void ismulhmImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        ctx.rf.r[instr.dst_register] = intrinsics::smulh(ctx.rf.r[instr.dst_register], ReadScratchpadValue<uint64_t>(ctx, instr, scratchpad));
    }

    static void imulrcpImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        ctx.rf.r[instr.dst_register] *= ctx.rcp[ctx.ic];
    }

    static void inegrImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        ctx.rf.r[instr.dst_register] = ~(ctx.rf.r[instr.dst_register]) + 1; // Two's complement negative.
    }

    // with imm
    static void ixorrImpl1(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        ctx.rf.r[instr.dst_register] ^= static_cast<int32_t>(instr.imm32);
    }

    // with src
    static void ixorrImpl2(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const uint64_t r_src_value{ ctx.rf.r[instr.src_register] }; // Integer register source value.
        ctx.rf.r[instr.dst_register] ^= r_src_value;
    }

    static void ixormImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        ctx.rf.r[instr.dst_register] ^= ReadScratchpadValue<uint64_t>(ctx, instr, scratchpad);
    }

    // with imm
    static void irorrImpl1(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        ctx.rf.r[instr.dst_register] = std::rotr(ctx.rf.r[instr.dst_register], instr.imm32);
    }

    // with src
    static void irorrImpl2(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const uint64_t r_src_value{ ctx.rf.r[instr.src_register] }; // Integer register source value.
        ctx.rf.r[instr.dst_register] = std::rotr(ctx.rf.r[instr.dst_register], r_src_value % 64);
    }

    // with imm
    static void irolrImpl1(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        ctx.rf.r[instr.dst_register] = std::rotl(ctx.rf.r[instr.dst_register], instr.imm32);
    }

    // with src
    static void irolrImpl2(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const uint64_t r_src_value{ ctx.rf.r[instr.src_register] }; // Integer register source value.
        ctx.rf.r[instr.dst_register] = std::rotl(ctx.rf.r[instr.dst_register], r_src_value % 64);
    }

    static void iswaprImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        std::swap(ctx.rf.r[instr.src_register], ctx.rf.r[instr.dst_register]);
    }

    // swap f
    static void fswaprImpl1(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };
        ctx.rf.f[f_dst_register] = intrinsics::sse::vswap<double>(ctx.rf.f[f_dst_register]);
    }

    // swap e
    static void fswaprImpl2(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };
        ctx.rf.e[f_dst_register] = intrinsics::sse::vswap<double>(ctx.rf.e[f_dst_register]);
    }

    static void faddrImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const auto f_src_register{ instr.src_register % Float_Register_Count };
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };
        ctx.rf.f[f_dst_register] = intrinsics::sse::vadd<double>(ctx.rf.f[f_dst_register], ctx.rf.a[f_src_register]);
    }

    static void faddmImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };
        ctx.rf.f[f_dst_register] = intrinsics::sse::vadd<double>(ctx.rf.f[f_dst_register], ReadScratchpadValue<intrinsics::xmm128d_t>(ctx, instr, scratchpad));
    }

    static void fsubrImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const auto f_src_register{ instr.src_register % Float_Register_Count };
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };
        ctx.rf.f[f_dst_register] = intrinsics::sse::vsub<double>(ctx.rf.f[f_dst_register], ctx.rf.a[f_src_register]);
    }

    static void fsubmImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };
        ctx.rf.f[f_dst_register] = intrinsics::sse::vsub<double>(ctx.rf.f[f_dst_register], ReadScratchpadValue<intrinsics::xmm128d_t>(ctx, instr, scratchpad));
    }

    static void fscalrImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };
        constexpr intrinsics::xmm128d_t Mask{
            std::bit_cast<double>(0x80F0000000000000),
            std::bit_cast<double>(0x80F0000000000000),
        };
        ctx.rf.f[f_dst_register] = intrinsics::sse::vxor<double>(ctx.rf.f[f_dst_register], Mask);
    }

    static void fmulrImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const auto f_src_register{ instr.src_register % Float_Register_Count };
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };
        ctx.rf.e[f_dst_register] = intrinsics::sse::vmul<double>(ctx.rf.e[f_dst_register], ctx.rf.a[f_src_register]);
    }

    static void fdivmImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const auto f_src_register{ instr.src_register % Float_Register_Count };
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };
        const auto f_src_value{ convertFloatRegister(ReadScratchpadValue<intrinsics::xmm128d_t>(ctx, instr, scratchpad), ctx.cfg.e_mask) };
        ctx.rf.e[f_dst_register] = intrinsics::sse::vdiv<double>(ctx.rf.e[f_dst_register], f_src_value);
    }

    static void fsqrtrImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };
        ctx.rf.e[f_dst_register] = intrinsics::sse::vsqrt<double>(ctx.rf.e[f_dst_register]);
    }

    static void cbranchImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        ctx.rf.r[instr.dst_register] += ctx.rcp[ctx.ic];
        if ((ctx.rf.r[instr.dst_register] & ctx.extra[ctx.ic]) == 0) {
            ctx.ic = ctx.branch_target[ctx.ic];
        }
    }

    static void istoreImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const uint64_t r_src_value{ ctx.rf.r[instr.src_register] }; // Integer register source value.
        constexpr uint32_t L3_Store_Condition{ 14 };
        const auto imm{ static_cast<int32_t>(instr.imm32) };

        auto mem_mask{ Scratchpad_L3_Mask };
        if (instr.modCond() < L3_Store_Condition) {
            mem_mask = instr.modMask() ? Scratchpad_L1_Mask : Scratchpad_L2_Mask;
        }

        const auto store_offset{ (ctx.rf.r[instr.dst_register] + imm) & mem_mask };
        scratchpad.write(store_offset, &r_src_value, sizeof(r_src_value));
    }

    static void cfroundImpl(ProgramContext& ctx, const RxInstruction& instr, Scratchpad& scratchpad) {
        const auto f_dst_register{ instr.dst_register % Float_Register_Count };
        const uint64_t r_src_value{ ctx.rf.r[instr.src_register] }; // Integer register source value.
        intrinsics::sse::setFloatRoundingMode(std::rotr(r_src_value, instr.imm32) % intrinsics::sse::Floating_Round_Modes);
    }

    void Interpreter::finalizeRegisters(ProgramContext& ctx) {
        ctx.mem.mx ^= ctx.rf.r[ctx.cfg.read_reg[2]] ^ ctx.rf.r[ctx.cfg.read_reg[3]];
        ctx.mem.mx &= Cache_Line_Align_Mask;
        intrinsics::prefetch<intrinsics::PrefetchMode::NTA, void>(dataset[(ctx.cfg.dataset_offset + ctx.mem.mx) / Cache_Line_Size].data());
        const auto dt_index{ (ctx.cfg.dataset_offset + ctx.mem.ma) / Cache_Line_Size };
        DatasetItem dt_item{ dataset[dt_index] };
        // Step 5.

        // Step 6. - omitted
        // Step 7. and step 9.
        // Step 8.
        std::swap(ctx.mem.mx, ctx.mem.ma);

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

        // Step 12.
        ctx.sp_addr.mx = 0;
        ctx.sp_addr.ma = 0;
    }

    ProgramContext::ProgramContext(RxProgram& program) noexcept {
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
            program.instructions[i].dst_register %= Int_Register_Count;
            program.instructions[i].src_register %= Int_Register_Count;

            const auto dst_register{ program.instructions[i].dst_register };
            const auto src_register{ program.instructions[i].src_register };

            switch (LUT_Opcode[program.instructions[i].opcode]) { using enum Bytecode;
            case IADD_RS:
            {
                constexpr uint8_t Displacement_Reg_Idx{ 5 };
                reg_usage[dst_register] = i;
                const bool with_imm{ dst_register == Displacement_Reg_Idx };
                const bool with_shift{ program.instructions[i].modShift() != 0 };

                if (!with_shift && !with_imm) {
                    op_impl[i] = iaddrsImpl1;
                } else if (!with_shift && with_imm) {
                    op_impl[i] = iaddrsImpl2;
                } else if (with_shift && !with_imm) {
                    op_impl[i] = iaddrsImpl3;
                } else {
                    op_impl[i] = iaddrsImpl4;
                }
                break;
            }
            case IADD_M:
                reg_usage[dst_register] = i;
                op_impl[i] = iaddmImpl;
                break;
            case ISUB_R:
            {
                reg_usage[dst_register] = i;
                const bool with_imm{ dst_register == src_register };
                if (with_imm) {
                    op_impl[i] = isubrImpl1;
                } else {
                    op_impl[i] = isubrImpl2;
                }
                break;
            }
            case ISUB_M:
                reg_usage[dst_register] = i;
                op_impl[i] = isubmImpl;
                break;
            case IMUL_R:
            {
                reg_usage[dst_register] = i;
                const bool with_imm{ dst_register == src_register };
                if (with_imm) {
                    op_impl[i] = imulrImpl1;
                } else {
                    op_impl[i] = imulrImpl2;
                }
                break;
            }
            case IMUL_M:
                reg_usage[dst_register] = i;
                op_impl[i] = imulmImpl;
                break;
            case IMULH_R:
                reg_usage[dst_register] = i;
                op_impl[i] = imulhrImpl;
                break;
            case IMULH_M:
                reg_usage[dst_register] = i;
                op_impl[i] = imulhmImpl;
                break;
            case ISMULH_R:
                reg_usage[dst_register] = i;
                op_impl[i] = ismulhrImpl;
                break;
            case ISMULH_M:
                reg_usage[dst_register] = i;
                op_impl[i] = ismulhmImpl;
                break;
            case INEG_R:
                reg_usage[dst_register] = i;
                op_impl[i] = inegrImpl;
                break;
            case IXOR_R:
            {
                reg_usage[dst_register] = i;
                const bool with_imm{ dst_register == src_register };
                if (with_imm) {
                    op_impl[i] = ixorrImpl1;
                } else {
                    op_impl[i] = ixorrImpl2;
                }
                break;
            }
            case IXOR_M:
                reg_usage[dst_register] = i;
                op_impl[i] = ixormImpl;
                break;
            case IROR_R:
            {
                program.instructions[i].imm32 %= 64;
                reg_usage[dst_register] = i;
                const bool with_imm{ dst_register == src_register };
                if (with_imm) {
                    op_impl[i] = irorrImpl1;
                } else {
                    op_impl[i] = irorrImpl2;
                }
                break;
            }
            case IROL_R:
            {
                program.instructions[i].imm32 %= 64;
                reg_usage[dst_register] = i;
                const bool with_imm{ dst_register == src_register };
                if (with_imm) {
                    op_impl[i] = irolrImpl1;
                } else {
                    op_impl[i] = irolrImpl2;
                }
                break;
            }
            case IMUL_RCP:
                if (program.instructions[i].imm32 != 0 && !std::has_single_bit(program.instructions[i].imm32)) {
                    reg_usage[dst_register] = i;
                    rcp[i] = reciprocal(program.instructions[i].imm32);
                    op_impl[i] = imulrcpImpl;
                } else {
                    op_impl[i] = (i == Rx_Program_Size - 1) ? nopImpl : callnextImpl;
                }
                
                break;
            case ISWAP_R:
                if (src_register != dst_register) {
                    reg_usage[dst_register] = i;
                    reg_usage[src_register] = i;
                    op_impl[i] = iswaprImpl;
                } else {
                    op_impl[i] = (i == Rx_Program_Size - 1) ? nopImpl : callnextImpl;
                }
                
                break;
            case CBRANCH:
            {
                constexpr uint32_t Condition_Mask{ (1 << Rx_Jump_Bits) - 1 };

                const auto shift{ program.instructions[i].modCond() + Rx_Jump_Offset};
                const auto mem_mask{ Condition_Mask << shift };

                uint64_t imm{ static_cast<int32_t>(program.instructions[i].imm32) | (1ULL << shift)};
                static_assert(Rx_Jump_Offset > 0, "Below simplification requires this assertion");
                imm &= ~(1ULL << (shift - 1)); // Clear the bit below the condition mask - this limits the number of successive jumps to 2.
                rcp[i] = imm;
                extra[i] = mem_mask;

                op_impl[i] = cbranchImpl;
                branch_target[i] = reg_usage[dst_register];
                for (auto& reg : reg_usage) { reg = i; } // Set all registers as used.
                break;
            }
            case FSWAP_R:
            {
                const bool swapf{ dst_register < Float_Register_Count };
                if (swapf) {
                    op_impl[i] = fswaprImpl1;
                } else {
                    op_impl[i] = fswaprImpl2;
                }
                break;
            }
            case FADD_R:
                op_impl[i] = faddrImpl;
                break;
            case FADD_M:
                op_impl[i] = faddmImpl;
                break;
            case FSUB_R:
                op_impl[i] = fsubrImpl;
                break;
            case FSUB_M:
                op_impl[i] = fsubmImpl;
                break;
            case FSCAL_R:
                op_impl[i] = fscalrImpl;
                break;
            case FMUL_R:
                op_impl[i] = fmulrImpl;
                break;
            case FDIV_M:
                op_impl[i] = fdivmImpl;
                break;
            case FSQRT_R:
                op_impl[i] = fsqrtrImpl;
                break;
            case CFROUND:
            {
                program.instructions[i].imm32 %= 64;
                op_impl[i] = cfroundImpl;
                break;
            }
            case ISTORE:
                op_impl[i] = istoreImpl;
                break;
            default:
                std::unreachable();
            }
        }
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

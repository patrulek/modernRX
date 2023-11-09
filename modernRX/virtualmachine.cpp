#include <algorithm>
#include <array>
#include <utility>

#include "aes1rhash.hpp"
#include "aes1rrandom.hpp"
#include "aes4rrandom.hpp"
#include "assembler.hpp"
#include "bytecodecompiler.hpp"
#include "randomxparams.hpp"
#include "sse.hpp"
#include "virtualmachine.hpp"
#include "virtualmem.hpp"

namespace modernRX {
    namespace {
        constexpr uint32_t Scratchpad_L3_Mask64{ (Rx_Scratchpad_L3_Size - 1) & ~63 }; // L3 cache 64-byte alignment mask.

        constexpr uint64_t Mantissa_Size{ 52 };
        constexpr uint64_t Cache_Line_Size{ sizeof(DatasetItem) };
        constexpr uint64_t Cache_Line_Align_Mask{ (Rx_Dataset_Base_Size - 1) & ~(Cache_Line_Size - 1) }; // Dataset 64-byte alignment mask.

        // Holds representation of register file used by VirtualMachine during program execution.
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

        constexpr uint32_t Emask_Offset{ offsetof(ProgramConfiguration, e_mask) };
        constexpr uint32_t Read_Reg_Offset{ offsetof(ProgramConfiguration, read_reg) };
        constexpr uint32_t Dataset_Offset{ offsetof(ProgramConfiguration, dataset_offset) };
        static_assert(Emask_Offset == 0, "This assert is required, because this value is also used in compiler's context");

        [[nodiscard]] constexpr double getSmallPositiveFloat(const uint64_t entropy) noexcept;
        [[nodiscard]] constexpr uint64_t getFloatRegisterMask(const uint64_t entropy) noexcept;
    }

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
        MemoryRegisters mem{};
        MemoryRegisters sp_addr{};
        uint32_t iter{ 0 };
        ProgramConfiguration cfg{};
    };


    void compilePrologue(assembler::Context& asmb);
    void compileInitializeRegisters(assembler::Context& asmb);
    void compileInstructions(assembler::Context& asmb, const RxProgram& program);
    void compileFinalizeRegisters(assembler::Context& asmb, const ProgramContext& ctx);
    void compileEpilogue(assembler::Context& asmb);

    constexpr uint32_t Rf_Offset{ offsetof(ProgramContext, rf) };
    constexpr uint32_t Mem_Offset{ offsetof(ProgramContext, mem) };
    constexpr uint32_t Sp_Addr_Offset{ offsetof(ProgramContext, sp_addr) };
    constexpr uint32_t Cfg_Offset{ offsetof(ProgramContext, cfg) };
    static_assert(Cfg_Offset == 280, "This assert is required, because this value is also used in compiler's context");


    VirtualMachine::VirtualMachine(std::span<std::byte, 64> seed, const_span<DatasetItem> dataset)
        : dataset(dataset), scratchpad(Rx_Scratchpad_L3_Size) {
        aes::fill1R(scratchpad.buffer(), seed);
        std::memcpy(this->seed.data(), seed.data(), seed.size());
    }

    std::array<std::byte, 32> VirtualMachine::execute() {
        // RandomX requires specific float environment before executing any program.
        // This RAII object will set proper float flags on creation and restore its values on destruction. 
        const intrinsics::sse::FloatEnvironment fenv{};

        for (uint32_t i = 0; i < Rx_Program_Count - 1; ++i) {
            auto [ctx, program] { generateProgram() };
            compileProgram(ctx, program);
            // https://github.com/tevador/RandomX/blob/master/doc/specs.md#462-loop-execution
            // Step 1-13.
            const auto jit_program{ reinterpret_cast<JITRxProgram>(jit.get()) };
            jit_program(reinterpret_cast<uintptr_t>(&ctx), reinterpret_cast<uintptr_t>(scratchpad.data()), reinterpret_cast<uintptr_t>(dataset.data()));
            blake2b::hash(seed, span_cast<std::byte, sizeof(ctx.rf)>(ctx.rf));
        }

        auto [ctx, program] { generateProgram() };
        compileProgram(ctx, program);

        // https://github.com/tevador/RandomX/blob/master/doc/specs.md#462-loop-execution
        // Step 1-13.
        const auto jit_program{ reinterpret_cast<JITRxProgram>(jit.get()) };
        jit_program(reinterpret_cast<uintptr_t>(&ctx), reinterpret_cast<uintptr_t>(scratchpad.data()), reinterpret_cast<uintptr_t>(dataset.data()));

        aes::hash1R(span_cast<std::byte, sizeof(ctx.rf.a)>(ctx.rf.a), span_cast<std::byte, Rx_Scratchpad_L3_Size>(scratchpad.data()));
        std::array<std::byte, 32> output{};
        blake2b::hash(output, span_cast<std::byte>(ctx.rf));

        return output;
    }

    std::pair<ProgramContext, RxProgram> VirtualMachine::generateProgram() {
        RxProgram program{};
        aes::fill4R(span_cast<std::byte>(program), seed);

        // Last 64 bytes of the program are now the new seed.

        return std::make_pair(ProgramContext{ program }, program);
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

    void VirtualMachine::compileProgram(const ProgramContext& ctx, const RxProgram& program) {
        using namespace assembler;
        using namespace assembler::registers;

        Context asmb(16 * 1024);

        // Compile prologue.
        compilePrologue(asmb);

        // Prefetch first dataset item.
        asmb.mov(RAX, RDI[Mem_Offset + 4], 1, false);
        asmb.add(RAX, RDI[Cfg_Offset + Dataset_Offset]);
        asmb.mov64(RDX, static_cast<uint32_t>(~(Cache_Line_Size - 1)));
        static_assert(std::has_single_bit(Cache_Line_Size));
        asmb.and_(RAX, RDX);
        asmb.prefetchnta(RBP[RAX[0]]);

        // Set loop entry.
        constexpr int Loop_Label{ Rx_Program_Size + 1 };
        static_assert(Loop_Label < 512); // 512 is max number of jmp labels in assembler::Context.
        asmb.label(Loop_Label);

        // Compile Initialize registers.
        compileInitializeRegisters(asmb);

        // Compile instructions.
        compileInstructions(asmb, program);

        // Compile finalize registers.
        compileFinalizeRegisters(asmb, ctx);

        // Decrease loop counter.
        asmb.sub(RBX, 1);
        asmb.jne(Loop_Label);
        
        // Compile epilogue.
        compileEpilogue(asmb);

        jit = makeExecutable<JITRxProgram>(asmb.flushCode(), asmb.flushData());
    }

    void compilePrologue(assembler::Context& asmb) {
        using namespace assembler;
        using namespace assembler::registers;

        asmb.push(RBP, RSP, RBX, RSI, RDI, R12, R13, R14, R15, XMM6, XMM7, XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15);

        // Scratchpad address passed in RDX register, move to RSI.
        asmb.mov(RSI, RDX);

        // Dataset address passed in R8 register, move to RBP.
        asmb.mov(RBP, R08);

        // Program context address passed in RCX register, move to RDI.
        asmb.mov(RDI, RCX);

        // Program iterations stored in RBX.
        asmb.mov(RBX, Rx_Program_Iterations);

        // Set Mantissa_Mask in XMM15 register.
        constexpr uint64_t Mantissa_Mask{ 0x00FF'FFFF'FFFF'FFFF };
        asmb.vpbroadcastq(XMM15, Mantissa_Mask);

        // Set Scale_Mask in XMM14 register.
        constexpr uint64_t Scale_Mask{ 0x80F0'0000'0000'0000 };
        asmb.vpbroadcastq(XMM14, Scale_Mask);

        // Load all registers from ctx object.
        for (int i = 0; i < 4; ++i) {
            asmb.xor_(Register::GPR(8 + i * 2), Register::GPR(8 + i * 2));
            asmb.xor_(Register::GPR(9 + i * 2), Register::GPR(9 + i * 2));
            asmb.vmovdqu(Register::XMM(i + 8), RCX[192 + i * 16]); // 'a' registers.
        }
    }

    // step.2 and step.3
    // Initializes registers before RandomX program execution.
    // Performs steps 2-3 defined by: https://github.com/tevador/RandomX/blob/master/doc/specs.md#462-loop-execution
    void compileInitializeRegisters(assembler::Context& asmb) {
        using namespace assembler;
        using namespace assembler::registers;

        // Step 2.
        asmb.mov(RAX, RDI[Sp_Addr_Offset], 1, false);
        asmb.mov(RCX, RDI[Sp_Addr_Offset + 4], 1, false);

        for (uint32_t i = 0; i < Int_Register_Count; ++i) {
            asmb.xor_(Register::GPR(8 + i), RSI[RAX[8 * i]]);
        }

        // Step 3.
        // "F-group" register initialization: https://github.com/tevador/RandomX/blob/master/doc/specs.md#431-group-f-register-conversion
        for (uint32_t i = 0; i < Float_Register_Count; ++i) {
            asmb.vcvtdq2pd(Register::XMM(i), RSI[RCX[8 * i]]);
        }

        // "E-group" register initialization: https://github.com/tevador/RandomX/blob/master/doc/specs.md#432-group-e-register-conversion
        for (uint32_t i = 0; i < Float_Register_Count; ++i) {
            asmb.vmovdqu(XMM13, RDI[Cfg_Offset + Emask_Offset]); // Load e_mask.
            asmb.vcvtdq2pd(XMM12, RSI[RCX[8 * (4 + i)]]);
            asmb.vpand(XMM12, XMM12, XMM15);
            asmb.vpor(Register::XMM(4 + i), XMM12, XMM13);
        }
    }

    void compileInstructions(assembler::Context& asmb, const RxProgram& program) {
        using namespace assembler;
        using namespace assembler::registers;

        std::array<int32_t, Int_Register_Count> reg_usage{};
        reg_usage.fill(-1);

        // Set label for program entry.
        asmb.label(0, false);

        // Compile all instructions.
        for (uint32_t i = 0; i < program.instructions.size(); ++i) {
            const RxInstruction& instr{ program.instructions[i] };
            LUT_Instr_Cmpl[instr.opcode](asmb, reg_usage, instr, i);
        }
    }

    void compileFinalizeRegisters(assembler::Context& asmb, const ProgramContext& ctx) {
        using namespace assembler;
        using namespace assembler::registers;

        // scratchpad -> RSI
        // dataset -> RBP
        // ctx -> RDI
        // Step. 5
        asmb.mov(RAX, RDI[Mem_Offset], 1, false);
        asmb.xor_(RAX, Register::GPR(8 | ctx.cfg.read_reg[2]));
        asmb.xor_(RAX, Register::GPR(8 | ctx.cfg.read_reg[3]));
        asmb.mov(RCX, Cache_Line_Align_Mask);
        asmb.and_(RAX, RCX);

        asmb.mov(RCX, RDI[Mem_Offset + 4], 1, false);
        asmb.mov(RDI[Mem_Offset + 4], RAX, 1, false);

        // Step.6 
        asmb.mov64(RDX, static_cast<uint32_t>(~(Cache_Line_Size - 1)));
        asmb.add(RAX, RDI[Cfg_Offset + Dataset_Offset]);
        static_assert(std::has_single_bit(Cache_Line_Size));
        asmb.and_(RAX, RDX);
        asmb.prefetchnta(RBP[RAX[0]]);

        // Step.7
        // ma loaded in step 5.
        asmb.mov(RDI[Mem_Offset], RCX, 1, false);
        asmb.add(RCX, RDI[Cfg_Offset + Dataset_Offset]);
        asmb.and_(RCX, RDX);

        // Step.8
        // mx stored in step 7.
        // ma stored in step 5.

        // Step.9
        asmb.xor_(R08, RBP[RCX[0]]);
        asmb.xor_(R09, RBP[RCX[8]]);
        asmb.xor_(R10, RBP[RCX[16]]);
        asmb.xor_(R11, RBP[RCX[24]]);
        asmb.xor_(R12, RBP[RCX[32]]);
        asmb.xor_(R13, RBP[RCX[40]]);
        asmb.xor_(R14, RBP[RCX[48]]);
        asmb.xor_(R15, RBP[RCX[56]]);

        asmb.mov(RAX, RDI[Sp_Addr_Offset + 4], 1, false);
        asmb.add(RAX, RSI);
        asmb.mov(RAX[0], R08, 1);
        asmb.mov(RAX[8], R09, 1);
        asmb.mov(RAX[16], R10, 1);
        asmb.mov(RAX[24], R11, 1);
        asmb.mov(RAX[32], R12, 1);
        asmb.mov(RAX[40], R13, 1);
        asmb.mov(RAX[48], R14, 1);
        asmb.mov(RAX[56], R15, 1);

        // Step. 10
        asmb.vpxor(XMM0, XMM0, XMM4);
        asmb.vpxor(XMM1, XMM1, XMM5);
        asmb.vpxor(XMM2, XMM2, XMM6);
        asmb.vpxor(XMM3, XMM3, XMM7);

        // Step. 11
        asmb.mov(RAX, RDI[Sp_Addr_Offset], 1, false);
        asmb.add(RAX, RSI);
        asmb.vmovdqu(RAX[0], XMM0);
        asmb.vmovdqu(RAX[16], XMM1);
        asmb.vmovdqu(RAX[32], XMM2);
        asmb.vmovdqu(RAX[48], XMM3);

        // Step 1.
        asmb.mov(RAX, Register::GPR(8 | ctx.cfg.read_reg[0]));
        asmb.xor_(RAX, Register::GPR(8 | ctx.cfg.read_reg[1]));
        asmb.mov(RCX, RAX);
        asmb.shr(RCX, 32);
        asmb.and_(RAX, Scratchpad_L3_Mask64);
        asmb.and_(RCX, Scratchpad_L3_Mask64);
        asmb.mov(RDI[Sp_Addr_Offset], RAX, 1, false);
        asmb.mov(RDI[Sp_Addr_Offset + 4], RCX, 1, false);
    }

    void compileEpilogue(assembler::Context& asmb) {
        using namespace assembler;
        using namespace assembler::registers;

        // Save registers in ctx object.
        for (int i = 0; i < 8; ++i) {
            asmb.mov(RDI[Rf_Offset + i * 8], Register::GPR(8 + i));

            if (i < 4) {
                asmb.vmovdqu(RDI[Rf_Offset + 64 + i * 16], Register::XMM(i)); // 'f' registers.
                asmb.vmovdqu(RDI[Rf_Offset + 128 + i * 16], Register::XMM(i + 4)); // 'e' registers.
            }
        }

        asmb.pop(XMM15, XMM14, XMM13, XMM12, XMM11, XMM10, XMM9, XMM8, XMM7, XMM6, R15, R14, R13, R12, RDI, RSI, RBX, RSP, RBP);
        asmb.ret();
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
    }
}

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
        constexpr uint32_t Max_Program_Size{ Rx_Program_Size * 32 };
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


    VirtualMachine::VirtualMachine() 
        : scratchpad(Rx_Scratchpad_L3_Size), asmb(Max_Program_Size) {
        jit = makeExecutable<JITRxProgram>(Max_Program_Size);
    }

    void VirtualMachine::reset(const_span<std::byte> input, const_span<DatasetItem> dataset) {
        this->dataset = dataset;
        blake2b::hash(seed, input);
        aes::fill1R(scratchpad.buffer(), seed);
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

        // Compile prologue.
        compilePrologue(asmb);

        // Prefetch first dataset item.
        static_assert(std::has_single_bit(Cache_Line_Size));
        static_assert(Mem_Offset == 256);
        static_assert(Cfg_Offset == 280);
        static_assert(Dataset_Offset == 32);
        asmb.inject(char_array(
            0x8B, 0x87, 0x04, 0x01, 0x00, 0x00,
            0x03, 0x87, 0x38, 0x01, 0x00, 0x00,
            0x83, 0xe0, 0xc0,
            0x48, 0x0F, 0x18, 0x44, 0x05, 0x00
        ));

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

        asmb.flushCode(jit.get());
    }

    void compilePrologue(assembler::Context& asmb) {
        using namespace assembler;
        using namespace assembler::registers;

        asmb.inject(char_array(
            0x53, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xec, 0x50,
            0xc5, 0xfa, 0x7f, 0x74, 0x24, 0x40, 0xc5, 0xfa, 0x7f, 0x7c, 0x24, 0x30, 0xc5, 0x7a, 0x7f, 0x44,
            0x24, 0x20, 0xc5, 0x7a, 0x7f, 0x4c, 0x24, 0x10, 0xc5, 0x7a, 0x7f, 0x14, 0x24, 0x48, 0x83, 0xec,
            0x50, 0xc5, 0x7a, 0x7f, 0x5c, 0x24, 0x40, 0xc5, 0x7a, 0x7f, 0x64, 0x24, 0x30, 0xc5, 0x7a, 0x7f,
            0x6c, 0x24, 0x20, 0xc5, 0x7a, 0x7f, 0x74, 0x24, 0x10, 0xc5, 0x7a, 0x7f, 0x3c, 0x24, 0x48, 0x8B,
            0xF2, 0x49, 0x8B, 0xE8, 0x48, 0x8B, 0xF9, 0x40, 0xBB,
            0x00, 0x08, 0x00, 0x00, 0x48, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xC4, 0x21,
            0xF9, 0x6E, 0xF8, 0xC4, 0x02, 0x79, 0x59, 0xFF, 0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xF0, 0x80, 0xC4, 0x21, 0xF9, 0x6E, 0xF0, 0xC4, 0x02, 0x79, 0x59, 0xF6, 0x4D, 0x33, 0xC0, 0x4D,
            0x33, 0xC9, 0xC5, 0x7A, 0x6F, 0x81, 0xC0, 0x00, 0x00, 0x00, 0x4D, 0x33, 0xD2, 0x4D, 0x33, 0xDB,
            0xC5, 0x7A, 0x6F, 0x89, 0xD0, 0x00, 0x00, 0x00, 0x4D, 0x33, 0xE4, 0x4D, 0x33, 0xED, 0xC5, 0x7A,
            0x6F, 0x91, 0xE0, 0x00, 0x00, 0x00, 0x4D, 0x33, 0xF6, 0x4D, 0x33, 0xFF, 0xC5, 0x7A, 0x6F, 0x99,
            0xF0, 0x00, 0x00, 0x00
        ));
    }

    // step.2 and step.3
    // Initializes registers before RandomX program execution.
    // Performs steps 2-3 defined by: https://github.com/tevador/RandomX/blob/master/doc/specs.md#462-loop-execution
    void compileInitializeRegisters(assembler::Context& asmb) {
        using namespace assembler;
        using namespace assembler::registers;

        static_assert(Emask_Offset == 0);
        static_assert(Cfg_Offset == 280);
        static_assert(Sp_Addr_Offset == 264);
        asmb.inject(char_array(
            0x8B, 0x87, 0x08, 0x01, 0x00, 0x00, 0x8B, 0x8F, 0x0C, 0x01, 0x00, 0x00, 0x4C, 0x33, 0x04, 0x06,
            0x4C, 0x33, 0x4C, 0x06, 0x08, 0xC5, 0xFA, 0xE6, 0x04, 0x0E, 0xC5, 0x7A, 0x6F, 0xAF, 0x18, 0x01,
            0x00, 0x00, 0xC5, 0x7A, 0xE6, 0x64, 0x0E, 0x20, 0xC4, 0x01, 0x19, 0xDB, 0xE7, 0xC4, 0x81, 0x19,
            0xEB, 0xE5, 0x4C, 0x33, 0x54, 0x06, 0x10, 0x4C, 0x33, 0x5C, 0x06, 0x18, 0xC5, 0xFA, 0xE6, 0x4C,
            0x0E, 0x08, 0xC5, 0x7A, 0x6F, 0xAF, 0x18, 0x01, 0x00, 0x00, 0xC5, 0x7A, 0xE6, 0x64, 0x0E, 0x28,
            0xC4, 0x01, 0x19, 0xDB, 0xE7, 0xC4, 0x81, 0x19, 0xEB, 0xED, 0x4C, 0x33, 0x64, 0x06, 0x20, 0x4C,
            0x33, 0x6C, 0x06, 0x28, 0xC5, 0xFA, 0xE6, 0x54, 0x0E, 0x10, 0xC5, 0x7A, 0x6F, 0xAF, 0x18, 0x01,
            0x00, 0x00, 0xC5, 0x7A, 0xE6, 0x64, 0x0E, 0x30, 0xC4, 0x01, 0x19, 0xDB, 0xE7, 0xC4, 0x81, 0x19,
            0xEB, 0xF5, 0x4C, 0x33, 0x74, 0x06, 0x30, 0x4C, 0x33, 0x7C, 0x06, 0x38, 0xC5, 0xFA, 0xE6, 0x5C,
            0x0E, 0x18, 0xC5, 0x7A, 0x6F, 0xAF, 0x18, 0x01, 0x00, 0x00, 0xC5, 0x7A, 0xE6, 0x64, 0x0E, 0x38,
            0xC4, 0x01, 0x19, 0xDB, 0xE7, 0xC4, 0x81, 0x19, 0xEB, 0xFD
        ));
    }

    void compileInstructions(assembler::Context& asmb, const RxProgram& program) {
        using namespace assembler;
        using namespace assembler::registers;

        std::array<int32_t, Int_Register_Count> reg_usage{};
        reg_usage.fill(-1);

        // Set label for program entry.
        asmb.label<false>(0);

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
        static_assert(Mem_Offset == 256);
        static_assert(Cache_Line_Align_Mask == 0x7FFF'FFC0);
        static_assert(std::has_single_bit(Cache_Line_Size));
        static_assert(Cfg_Offset == 280);
        static_assert(Dataset_Offset == 32);
        static_assert(Sp_Addr_Offset == 264);
        static_assert(Scratchpad_L3_Mask64 == 0x001F'FFC0);

        asmb.inject(char_array(
            0x8b, 0x87, 0x00, 0x01, 0x00, 0x00, 0x44, 0x31, 0xc0 + 8 * (int)ctx.cfg.read_reg[2], 0x44,
            0x31, 0xc0 + 8 * (int)ctx.cfg.read_reg[3], 0x25, 0xc0, 0xff, 0xff, 0x7f, 0x8b, 0x8f, 0x04, 0x01, 0x00, 0x00, 0x89, 0x87, 0x04,
            0x01, 0x00, 0x00, 0x03, 0x87, 0x38, 0x01, 0x00, 0x00, 0x83, 0xe0, 0xc0, 0x48, 0x0f, 0x18, 0x04,
            0x28, 0x89, 0x8f, 0x00, 0x01, 0x00, 0x00, 0x03, 0x8f, 0x38, 0x01, 0x00, 0x00, 0x83, 0xe1, 0xc0,
            0x8b, 0x87, 0x0c, 0x01, 0x00, 0x00, 0x48, 0x01, 0xf0, 0x4c, 0x33, 0x44, 0x0d, 0x00, 0x4c, 0x33,
            0x4c, 0x0d, 0x08, 0x4c, 0x33, 0x54, 0x0d, 0x10, 0x4c, 0x33, 0x5c, 0x0d, 0x18, 0x4c, 0x89, 0x00,
            0x4c, 0x89, 0x48, 0x08, 0x4c, 0x33, 0x64, 0x0d, 0x20, 0x4c, 0x33, 0x6c, 0x0d, 0x28, 0x4c, 0x89,
            0x50, 0x10, 0x4c, 0x89, 0x58, 0x18, 0x4c, 0x33, 0x74, 0x0d, 0x30, 0x4c, 0x33, 0x7c, 0x0d, 0x38,
            0x4c, 0x89, 0x60, 0x20, 0x4c, 0x89, 0x68, 0x28, 0xc5, 0xf9, 0xef, 0xc4, 0xc5, 0xf1, 0xef, 0xcd,
            0xc5, 0xe9, 0xef, 0xd6, 0xc5, 0xe1, 0xef, 0xdf, 0x4c, 0x89, 0x70, 0x30, 0x4c, 0x89, 0x78, 0x38,
            0x8b, 0x87, 0x08, 0x01, 0x00, 0x00, 0x48, 0x01, 0xf0, 0xc5, 0xfa, 0x7f, 0x00, 0xc5, 0xfa, 0x7f,
            0x48, 0x10, 0xc5, 0xfa, 0x7f, 0x50, 0x20, 0xc5, 0xfa, 0x7f, 0x58, 0x30, 0x4c, 0x89, 0xc0 + 8 * (int)ctx.cfg.read_reg[0], 0x4c,
            0x31, 0xc0 + 8 * (int)ctx.cfg.read_reg[1], 0x48, 0x89, 0xc1, 0x48, 0xc1, 0xe9, 0x20, 0x48, 0x25, 0xc0, 0xff, 0x1f, 0x00, 0x48,
            0x81, 0xe1, 0xc0, 0xff, 0x1f, 0x00, 0x89, 0x87, 0x08, 0x01, 0x00, 0x00, 0x89, 0x8f, 0x0c, 0x01,
            0x00, 0x00
        ));
    }

    void compileEpilogue(assembler::Context& asmb) {
        using namespace assembler;
        using namespace assembler::registers;

        static_assert(Rf_Offset == 0);
        asmb.inject(char_array(
            0x4C, 0x89, 0x07, 0x4C, 0x89, 0x4F, 0x08, 0xC5, 0xFA, 0x7F, 0x47, 0x40, 0xC5, 0xFA, 0x7F, 0xA7,
            0x80, 0x00, 0x00, 0x00, 0x4C, 0x89, 0x57, 0x10, 0x4C, 0x89, 0x5F, 0x18, 0xC5, 0xFA, 0x7F, 0x4F,
            0x50, 0xC5, 0xFA, 0x7F, 0xAF, 0x90, 0x00, 0x00, 0x00, 0x4C, 0x89, 0x67, 0x20, 0x4C, 0x89, 0x6F,
            0x28, 0xC5, 0xFA, 0x7F, 0x57, 0x60, 0xC5, 0xFA, 0x7F, 0xB7, 0xA0, 0x00, 0x00, 0x00, 0x4C, 0x89,
            0x77, 0x30, 0x4C, 0x89, 0x7F, 0x38, 0xC5, 0xFA, 0x7F, 0x5F, 0x70, 0xC5, 0xFA, 0x7F, 0xBF, 0xB0,
            0x00, 0x00, 0x00, 0xc5, 0x7a, 0x6f, 0x5c, 0x24, 0x40, 0xc5, 0x7a, 0x6f, 0x64, 0x24, 0x30, 0xc5,
            0x7a, 0x6f, 0x6c, 0x24, 0x20, 0xc5, 0x7a, 0x6f, 0x74, 0x24, 0x10, 0xc5, 0x7a, 0x6f, 0x3c, 0x24,
            0x48, 0x83, 0xc4, 0x50, 0xc5, 0xfa, 0x6f, 0x74, 0x24, 0x40, 0xc5, 0xfa, 0x6f, 0x7c, 0x24, 0x30,
            0xc5, 0x7a, 0x6f, 0x44, 0x24, 0x20, 0xc5, 0x7a, 0x6f, 0x4c, 0x24, 0x10, 0xc5, 0x7a, 0x6f, 0x14,
            0x24, 0x48, 0x83, 0xc4, 0x50, 0x41, 0x5F, 0x41, 0x5E, 0x41, 0x5D, 0x41, 0x5C, 0x5F, 0x5E, 0x5D,
            0x5B, 0xC3
        ));
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

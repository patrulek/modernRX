#include <algorithm>
#include <array>
#include <utility>

#include "aes1rhash.hpp"
#include "aes1rrandom.hpp"
#include "aes4rrandom.hpp"
#include "bytecodecompiler.hpp"
#include "randomxparams.hpp"
#include "sse.hpp"
#include "virtualmachine.hpp"
#include "virtualmem.hpp"

#include "virtualmachineprogram.cpp"

namespace modernRX {
    namespace {
        constexpr uint32_t Scratchpad_L3_Mask64{ (Rx_Scratchpad_L3_Size - 1) & ~63 }; // L3 cache 64-byte alignment mask.
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
    }

    // Holds RandomX program entropy and instructions: https://github.com/tevador/RandomX/blob/master/doc/specs.md#44-program-buffer
    // Initialized by AES-filled buffer. Must preserve order of fields.
    struct alignas(64) RxProgram {
        std::array<uint64_t, 16> entropy{};
        std::array<RxInstruction, Rx_Program_Size> instructions{};
    };
    static_assert(sizeof(RxProgram) == Rx_Program_Bytes_Size); // Size of random program is also used in different context. Make sure both values match.
    static_assert(offsetof(RxProgram, entropy) == 0);


    VirtualMachine::VirtualMachine() 
        : scratchpad(Rx_Scratchpad_L3_Size + sizeof(RegisterFile)) {
        jit = makeExecutable<JITRxProgram>(sizeof(Code_Buffer));
        std::memcpy(jit.get(), Code_Buffer.data(), sizeof(Code_Buffer));
        compiler.code_buffer = reinterpret_cast<char*>(jit.get()) + Program_Offset;
    }

    void VirtualMachine::reset(const_span<std::byte> input, const_span<DatasetItem> dataset) {
        this->dataset = dataset;
        blake2b::hash(seed, input);
        aes::fill1R(scratchpad.buffer(sizeof(RegisterFile), scratchpad.size() - sizeof(RegisterFile)), seed);
    }

    std::array<std::byte, 32> VirtualMachine::execute() {
        // RandomX requires specific float environment before executing any program.
        // This RAII object will set proper float flags on creation and restore its values on destruction. 
        const intrinsics::sse::FloatEnvironment fenv{};
        RxProgram program;
        constexpr uint64_t Dataset_Extra_Items{ Rx_Dataset_Extra_Size / sizeof(DatasetItem) };
        static_assert(std::has_single_bit(Dataset_Extra_Items + 1));
        static_assert(Cache_Line_Size == 64);

        for (uint32_t i = 0; i < Rx_Program_Count - 1; ++i) {
            generateProgram(program);
            compileProgram(program);

            const auto dataset_offset{ (program.entropy[13] & Dataset_Extra_Items) * Cache_Line_Size };
            const auto dataset_ptr{ reinterpret_cast<uintptr_t>(dataset.data()) + dataset_offset };
            // https://github.com/tevador/RandomX/blob/master/doc/specs.md#462-loop-execution
            // Step 1-13.
            const auto jit_program{ reinterpret_cast<JITRxProgram>(jit.get()) };
            jit_program(0 /* deprecated */, reinterpret_cast<uintptr_t>(scratchpad.data()) + sizeof(RegisterFile),
                        dataset_ptr, reinterpret_cast<uintptr_t>(&program));
            blake2b::hash(seed, span_cast<std::byte, sizeof(RegisterFile)>(scratchpad.data()));
        }

        generateProgram(program);
        compileProgram(program); 

        // https://github.com/tevador/RandomX/blob/master/doc/specs.md#462-loop-execution
        // Step 1-13.
        const auto dataset_offset{ (program.entropy[13] & Dataset_Extra_Items) * Cache_Line_Size };
        const auto dataset_ptr{ reinterpret_cast<uintptr_t>(dataset.data()) + dataset_offset };
        const auto jit_program{ reinterpret_cast<JITRxProgram>(jit.get()) };
        jit_program(0 /* deprecated */, reinterpret_cast<uintptr_t>(scratchpad.data()) + sizeof(RegisterFile),
            dataset_ptr, reinterpret_cast<uintptr_t>(&program));

        aes::hash1R(span_cast<std::byte, sizeof(RegisterFile::a)>(scratchpad.data() + sizeof(RegisterFile) - sizeof(RegisterFile::a)), 
            span_cast<std::byte, Rx_Scratchpad_L3_Size>(scratchpad.data() + sizeof(RegisterFile)));
        
        std::array<std::byte, 32> output{};
        blake2b::hash(output, span_cast<std::byte, sizeof(RegisterFile)>(scratchpad.data()));

        return output;
    }

    void VirtualMachine::generateProgram(RxProgram& program) {
        aes::fill4R(span_cast<std::byte>(program), seed);
        // Last 64 bytes of the program are now the new seed.
    }

    void VirtualMachine::compileProgram(const RxProgram& program) {
        compiler.reset();

        // RDI = rf
        // RSI = scratchpad
        // RBP = dataset

        // Compile all instructions.
        for (uint32_t i = 0; i < program.instructions.size(); ++i) {
            compiler.instr_offset[i] = compiler.code_size;
            const RxInstruction& instr{ program.instructions[i] };
            (compiler.*LUT_Instr_Cmpl[instr.opcode])(instr, i);
        }

        auto loop_finalization_offset{ 0 };
        // Program size thresholds
        // Jmp to finalize registers.
        if (compiler.code_size < Loop_Finalization_Offset_1 - Program_Offset - 5) {
            loop_finalization_offset = Loop_Finalization_Offset_1;
        } else if (compiler.code_size < Loop_Finalization_Offset_2 - Program_Offset - 5) {
            loop_finalization_offset = Loop_Finalization_Offset_2;
        } else if (compiler.code_size < Max_Program_Size - 5) {
            loop_finalization_offset = Loop_Finalization_Offset_3;
        } else {
            // do nothing; just let it pass through nops
        }

        if (loop_finalization_offset > 0) {
            const int32_t jmp_offset{ loop_finalization_offset - compiler.code_size - Program_Offset - 5 };
            const uint8_t jmp{ 0xe9 };
            std::memcpy(compiler.code_buffer + compiler.code_size, &jmp, sizeof(uint8_t));
            std::memcpy(compiler.code_buffer + compiler.code_size + 1, &jmp_offset, sizeof(int32_t));
            compiler.code_size += 5;
        }

        // Compile finalize registers.
        static_assert(Cache_Line_Align_Mask == 0x7FFF'FFC0);
        static_assert(std::has_single_bit(Cache_Line_Size));
        static_assert(Scratchpad_L3_Mask64 == 0x001F'FFC0);

        const uint8_t read_reg2 = static_cast<uint8_t>(0xc7 + 8 * (4 + ((program.entropy[12] >> 2) & 1)));
        const auto rr2_offset{ 8 };
        std::memcpy(reinterpret_cast<char*>(jit.get()) + loop_finalization_offset + rr2_offset, &read_reg2, sizeof(uint8_t));

        const uint8_t read_reg3 = static_cast<uint8_t>(0xc7 + 8 * (6 + ((program.entropy[12] >> 3) & 1)));
        const auto rr3_offset{ 11 };
        std::memcpy(reinterpret_cast<char*>(jit.get()) + loop_finalization_offset + rr3_offset, &read_reg3, sizeof(uint8_t));

        const uint8_t read_reg0 = static_cast<uint8_t>(0xc2 + 8 * (0 + ((program.entropy[12] >> 0) & 1)));
        const auto rr0_offset{ 149 };
        std::memcpy(reinterpret_cast<char*>(jit.get()) + loop_finalization_offset + rr0_offset, &read_reg0, sizeof(uint8_t));

        const uint8_t read_reg1 = static_cast<uint8_t>(0xc2 + 8 * (2 + ((program.entropy[12] >> 1) & 1)));
        const auto rr1_offset{ 152 };
        std::memcpy(reinterpret_cast<char*>(jit.get()) + loop_finalization_offset + rr1_offset, &read_reg1, sizeof(uint8_t));
    }
}

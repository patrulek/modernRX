#include <algorithm>
#include <array>
#include <utility>

#include "aes1rhash.hpp"
#include "aes1rrandom.hpp"
#include "aes4rrandom.hpp"
#include "bytecodecompiler.hpp"
#include "hash.hpp"
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
    }

    // Holds RandomX program entropy and instructions: https://github.com/tevador/RandomX/blob/master/doc/specs.md#44-program-buffer
    // Initialized by AES-filled buffer. Must preserve order of fields.
    struct alignas(64) RxProgram {
        std::array<uint64_t, 16> entropy{};
        std::array<RxInstruction, Rx_Program_Size> instructions{};
    };
    static_assert(sizeof(RxProgram) == Rx_Program_Bytes_Size); // Size of random program is also used in different context. Make sure both values match.
    static_assert(offsetof(RxProgram, entropy) == 0);


    VirtualMachine::VirtualMachine(std::span<std::byte, Required_Memory> scratchpad) 
        : scratchpad(scratchpad) {
        jit = makeExecutable<JITRxProgram>(sizeof(Code_Buffer));
        std::memcpy(jit.get(), Code_Buffer.data(), sizeof(Code_Buffer));
        compiler.code_buffer = reinterpret_cast<char*>(jit.get()) + Program_Offset;
    }

    void VirtualMachine::reset(BlockTemplate block_template, const_span<DatasetItem> dataset) noexcept {
        this->dataset = dataset;
        this->block_template = block_template;
        this->new_block_template = true;
    }

    void VirtualMachine::executeNext(std::function<void(const RxHash&)> callback) noexcept{
        // RandomX requires specific float environment before executing any program.
        // This RAII object will set proper float flags on creation and restore its values on destruction. 
        const intrinsics::sse::FloatEnvironment fenv{};
        RxProgram program;
        constexpr uint64_t Dataset_Extra_Items{ Rx_Dataset_Extra_Size / sizeof(DatasetItem) };
        static_assert(Dataset_Extra_Items == 524'287);
        static_assert(Cache_Line_Size == 64);

        for (uint32_t i = 0; i < Rx_Program_Count - 1; ++i) {
            generateProgram(program);
            compileProgram(program);

            const auto dataset_ptr{ reinterpret_cast<uintptr_t>(dataset.data()) };
            // https://github.com/tevador/RandomX/blob/master/doc/specs.md#462-loop-execution
            // Step 1-13.
            const auto jit_program{ reinterpret_cast<JITRxProgram>(jit.get()) };
            jit_program(reinterpret_cast<uintptr_t>(scratchpad.data()) + sizeof(RegisterFile), dataset_ptr, reinterpret_cast<uintptr_t>(&program));
            blake2b::hash(seed, span_cast<std::byte, sizeof(RegisterFile)>(scratchpad.data()));
        }

        generateProgram(program);
        compileProgram(program);

        // https://github.com/tevador/RandomX/blob/master/doc/specs.md#462-loop-execution
        // Step 1-13.
        const auto dataset_ptr{ reinterpret_cast<uintptr_t>(dataset.data()) };
        const auto jit_program{ reinterpret_cast<JITRxProgram>(jit.get()) };
        jit_program(reinterpret_cast<uintptr_t>(scratchpad.data()) + sizeof(RegisterFile), dataset_ptr, reinterpret_cast<uintptr_t>(&program));


        // Hash and fill for next iteration.
        block_template.next();
        blake2b::hash(seed, block_template.view());
        auto rfa_view{ span_cast<std::byte, sizeof(RegisterFile::a)>(scratchpad.data() + sizeof(RegisterFile) - sizeof(RegisterFile::a)) };
        auto scratchpad_view{ scratchpad.subspan(sizeof(RegisterFile), Rx_Scratchpad_L3_Size) };
        aes::hashAndFill1R(rfa_view, seed, scratchpad_view);

        // Get final hash.
        alignas(32) RxHash output;
        auto rf_view{ span_cast<std::byte, sizeof(RegisterFile)>(scratchpad.data()) };
        blake2b::hash(output.buffer(), rf_view);
        callback(output);
    }

    void VirtualMachine::execute(std::function<void(const RxHash&)> callback) noexcept {
        if (!new_block_template) {
            executeNext(callback);
            return;
        }

        new_block_template = false;

        // Initialize scratchpad.
        blake2b::hash(seed, block_template.view());
        aes::fill1R(scratchpad.subspan(sizeof(RegisterFile), Rx_Scratchpad_L3_Size), seed);
    }

    void VirtualMachine::generateProgram(RxProgram& program) noexcept {
        aes::fill4R(span_cast<std::byte>(program), seed);
        // Last 64 bytes of the program are now the new seed.
    }

    void VirtualMachine::compileProgram(const RxProgram& program) noexcept {
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

        constexpr auto loop_finalization_offset{ Loop_Finalization_Offset_3 };
        constexpr auto Jmp_Code_Size{ 5 };
        constexpr uint8_t nops[16]{ 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

        if (compiler.code_size < Max_Program_Size - Jmp_Code_Size) {
            // do the jmp
            const auto jmp_pos{ Program_Offset + compiler.code_size + Jmp_Code_Size };
            const int32_t jmp_offset{ loop_finalization_offset - jmp_pos };
            const uint8_t jmp{ 0xe9 };
            std::memcpy(compiler.code_buffer + compiler.code_size, &jmp, sizeof(uint8_t));
            std::memcpy(compiler.code_buffer + compiler.code_size + 1, &jmp_offset, sizeof(int32_t));
            compiler.code_size += Jmp_Code_Size;
            const auto nop_size{ std::min<uint32_t>(sizeof(nops), Max_Program_Size - compiler.code_size)};
            std::memcpy(compiler.code_buffer + compiler.code_size, nops, nop_size);
            compiler.code_size += nop_size;
        } else {
            // fill with nops
            std::memcpy(compiler.code_buffer + compiler.code_size, nops, Max_Program_Size - compiler.code_size);
            compiler.code_size += Max_Program_Size - compiler.code_size;
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

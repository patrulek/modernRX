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
#include "trace.hpp"
#include "virtualmachine.hpp"
#include "virtualmem.hpp"

#include "virtualmachineprogram.cpp"

namespace modernRX {
    namespace {
        constexpr uint32_t Scratchpad_L3_Mask64{ (Rx_Scratchpad_L3_Size - 1) & ~63 }; // L3 cache 64-byte alignment mask.
        constexpr uint64_t Cache_Line_Size{ sizeof(DatasetItem) };
        constexpr uint64_t Cache_Line_Align_Mask{ (Rx_Dataset_Base_Size - 1) & ~(Cache_Line_Size - 1) }; // Dataset 64-byte alignment mask.
        constexpr auto Rf_Offset{ 0 };
        constexpr auto Sp_Offset{ Rf_Offset + 256 };
    }

    VirtualMachine::VirtualMachine(std::span<std::byte, Required_Memory> scratchpad, JITRxProgram jit, const uint32_t vm_id)
        : memory(scratchpad), jit(jit) {
        std::memcpy(jit, Code_Buffer.data(), sizeof(Code_Buffer));
        compiler.code_buffer = reinterpret_cast<char*>(jit) + Program_Offset;
        pdata.vm_id = vm_id;
    }

    void VirtualMachine::reset(BlockTemplate block_template, const_span<DatasetItem> dataset) noexcept {
        this->dataset = dataset;
        this->block_template = block_template;
        this->new_block_template = true;
        this->pdata.hashes = 0;
    }

    struct alignas(16) OtherConsts {
        uint64_t scratchpad_offset_mask{ 0x001f'ffc0'001f'ffc0 };
        uint64_t reserved{};
    };

    struct alignas(16) FloatingEnv {
        uint32_t mode0{ 0x9fc0 };
        uint32_t mode1{ 0xbfc0 };
        uint32_t mode2{ 0xdfc0 };
        uint32_t mode3{ 0xffc0 };
    };

    FloatingEnv global_fenv{};

    void VirtualMachine::executeNext(std::function<void(const RxHash&)> callback) noexcept{
        const intrinsics::sse::FloatEnvironment fenv{};
        constexpr uint64_t Dataset_Extra_Items{ Rx_Dataset_Extra_Size / sizeof(DatasetItem) };
        static_assert(Dataset_Extra_Items == 524'287);
        static_assert(Cache_Line_Size == 64);

        const auto dataset_ptr{ reinterpret_cast<uintptr_t>(dataset.data()) };
        const auto rf_ptr{ reinterpret_cast<uintptr_t>(memory.data()) + Rf_Offset };
        const auto scratchpad_ptr{ reinterpret_cast<uintptr_t>(memory.data()) + Sp_Offset };
        const auto global_ptr{ static_cast<uintptr_t>(0) }; // Currently unused.

        OtherConsts* other_consts_ptr{ reinterpret_cast<OtherConsts*>(scratchpad_ptr - 32) };
        FloatingEnv* fenv_ptr{ reinterpret_cast<FloatingEnv*>(scratchpad_ptr - 16) };

        RxProgram program;
        const auto program_ptr{ reinterpret_cast<uintptr_t>(&program) };
        compiler.program = &program;

        for (uint32_t i = 0; i < Rx_Program_Count - 1; ++i) {
            generateProgram(program);
            compileProgram(program);

            *other_consts_ptr = OtherConsts{};
            *fenv_ptr = FloatingEnv{};
            jit(scratchpad_ptr, dataset_ptr, program_ptr, global_ptr);

            blake2b::hash(seed, span_cast<std::byte, sizeof(RegisterFile)>(reinterpret_cast<std::byte*>(rf_ptr)));
        }

        {
            Trace<TraceEvent::Generate> _;
            generateProgram(program);
        }

        {
            Trace<TraceEvent::Compile> _;
            compileProgram(program);
        }


        {
            Trace<TraceEvent::Execute> _;
            *other_consts_ptr = OtherConsts{};
            *fenv_ptr = FloatingEnv{};
            jit(scratchpad_ptr, dataset_ptr, program_ptr, global_ptr);
        }


        {
            Trace<TraceEvent::HashAndFill> _;

            // Hash and fill for next iteration.
            block_template.next();
            blake2b::hash(seed, block_template.view());

            const auto rfa_view{ span_cast<std::byte, sizeof(RegisterFile::a)>(reinterpret_cast<std::byte*>(scratchpad_ptr - sizeof(RegisterFile::a))) };
            const auto scratchpad_view{ std::span<std::byte>(reinterpret_cast<std::byte*>(scratchpad_ptr), Rx_Scratchpad_L3_Size) };
            aes::hashAndFill1R(rfa_view, seed, scratchpad_view);

            // Get final hash.
            const auto rf_view{ span_cast<std::byte, sizeof(RegisterFile)>(reinterpret_cast<std::byte*>(rf_ptr)) };
            blake2b::hash(output.buffer(), rf_view);
            ++pdata.hashes;
        }

        callback(output);
    }

    void VirtualMachine::execute(std::function<void(const RxHash&)> callback) noexcept {
        if (!new_block_template) {
            executeNext(callback);
            return;
        }

        new_block_template = false;
        const auto scratchpad_ptr{ reinterpret_cast<uintptr_t>(memory.data()) + Sp_Offset };
        const auto scratchpad_view{ std::span<std::byte>(reinterpret_cast<std::byte*>(scratchpad_ptr), Rx_Scratchpad_L3_Size) };

        // Initialize memory.
        blake2b::hash(seed, block_template.view());
        aes::fill1R(scratchpad_view, seed);
    }

    void VirtualMachine::generateProgram(RxProgram& program) noexcept {
        intrinsics::prefetch<intrinsics::PrefetchMode::T0, 1>(&compiler.Base_Cmpl_Addr);
        for (int i = 0; i < 8; ++i) {
            intrinsics::prefetch<intrinsics::PrefetchMode::T0, 1>(LUT_Instr_Cmpl_Offsets.data() + 32);
            intrinsics::prefetch<intrinsics::PrefetchMode::T0, 1>(compiler.instr_offset.data() + 32);
        }
        aes::fill4R(span_cast<std::byte>(program), seed);
        // Last 64 bytes of the program are now the new seed.
    }

    void VirtualMachine::compileProgram(const RxProgram& program) noexcept {
        compiler.reset();

        // RDI = rf
        // RSI = memory
        // RBP = dataset
        

        // Compile all instructions.
        for (uint32_t i = 0; i < program.instructions.size(); ) {
            const RxInstruction& instr{ program.instructions[i++] };
            const RxInstruction& instr2{ program.instructions[i++] };
            const RxInstruction& instr3{ program.instructions[i++] };
            const RxInstruction& instr4{ program.instructions[i++] };

            const auto cmpl_func{ ForceCast<InstrCmpl>(compiler.Base_Cmpl_Addr + LUT_Instr_Cmpl_Offsets[instr.opcode]) };
            const auto cmpl_func2{ ForceCast<InstrCmpl>(compiler.Base_Cmpl_Addr + LUT_Instr_Cmpl_Offsets[instr2.opcode]) };
            const auto cmpl_func3{ ForceCast<InstrCmpl>(compiler.Base_Cmpl_Addr + LUT_Instr_Cmpl_Offsets[instr3.opcode]) };
            const auto cmpl_func4{ ForceCast<InstrCmpl>(compiler.Base_Cmpl_Addr + LUT_Instr_Cmpl_Offsets[instr4.opcode]) };
            i -= 4;

            compiler.instr_offset[i] = compiler.code_size;
            (compiler.*cmpl_func)(instr, i++);

            compiler.instr_offset[i] = compiler.code_size;
            (compiler.*cmpl_func2)(instr2, i++);

            compiler.instr_offset[i] = compiler.code_size;
            (compiler.*cmpl_func3)(instr3, i++);

            compiler.instr_offset[i] = compiler.code_size;
            (compiler.*cmpl_func4)(instr4, i++);
        }

        constexpr auto loop_finalization_offset{ Loop_Finalization_Offset_3 };
        constexpr auto Jmp_Code_Size{ 5 };
        constexpr uint8_t nops[8]{ 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

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
        const auto rr2_offset{ 18 };
        std::memcpy(reinterpret_cast<char*>(jit) + loop_finalization_offset + rr2_offset, &read_reg2, sizeof(uint8_t));

        const uint8_t read_reg3 = static_cast<uint8_t>(0xc7 + 8 * (6 + ((program.entropy[12] >> 3) & 1)));
        const auto rr3_offset{ 21 };
        std::memcpy(reinterpret_cast<char*>(jit) + loop_finalization_offset + rr3_offset, &read_reg3, sizeof(uint8_t));

        const uint8_t read_reg0 = static_cast<uint8_t>(0xc2 + 8 * (0 + ((program.entropy[12] >> 0) & 1)));
        const auto rr0_offset{ 155 };
        std::memcpy(reinterpret_cast<char*>(jit) + loop_finalization_offset + rr0_offset, &read_reg0, sizeof(uint8_t));

        const uint8_t read_reg1 = static_cast<uint8_t>(0xc2 + 8 * (2 + ((program.entropy[12] >> 1) & 1)));
        const auto rr1_offset{ 158 };
        std::memcpy(reinterpret_cast<char*>(jit) + loop_finalization_offset + rr1_offset, &read_reg1, sizeof(uint8_t));
    }
}

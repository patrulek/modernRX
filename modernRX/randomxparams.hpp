#pragma once

/*
* RandomX parameters configuration based on: https://github.com/tevador/RandomX/blob/master/doc/configuration.md
*/

#include <array>

#include "cast.hpp"

namespace modernRX {
    // Blockchain Parameters.
    inline constexpr uint32_t Rx_Block_Template_Size{ 76 }; // Size of the block template in bytes.

    // Argon2d Parameters.
    inline constexpr std::array<std::byte, 8> Rx_Argon2d_Salt{ byte_array(0x52, 0x61, 0x6e, 0x64, 0x6f, 0x6d, 0x58, 0x03) }; // RandomX\x03
    inline constexpr uint32_t Rx_Argon2d_Parallelism{ 1 }; // The number of parallel lanes for cache initialization.
    inline constexpr uint32_t Rx_Argon2d_Tag_Length{ 0 }; // No need to use tag for RandomX.
    inline constexpr uint32_t Rx_Argon2d_Memory_Blocks{ 262144 }; // The number of 1KB blocks in the cache.
    inline constexpr uint32_t Rx_Argon2d_Iterations{ 3 }; // The number of iterations for cache initialization.
    inline constexpr uint32_t Rx_Argon2d_Max_Parallelism = 16'777'215; // Maximum number of lanes. 
    inline constexpr uint32_t Rx_Argon2d_Type{ 0 }; // RandomX uses Argon2d algorithm, thus it needs to be 0.
    inline constexpr uint32_t Rx_Argon2d_Version{ 0x13 }; // RandomX uses Argon2 version 0x13.
    inline constexpr std::array<std::byte, 0> Rx_Argon2d_Secret{}; // No need to use secret for RandomX.
    inline constexpr std::array<std::byte, 0> Rx_Argon2d_Data{}; // No need to use additional data for RandomX.

    static_assert(Rx_Argon2d_Parallelism - 1 <= Rx_Argon2d_Max_Parallelism - 1, "Number of parallel lanes must be in range 1 - 16'777'215");
    static_assert(Rx_Argon2d_Memory_Blocks >= 8 * Rx_Argon2d_Parallelism, "Number of memory blocks cannot be lesser than 8 * number of parallel lanes.");
    static_assert(Rx_Argon2d_Memory_Blocks <= 2'097'152, "Total memory size of memory blocks cannot exceed 2GB.");
    static_assert(std::has_single_bit(Rx_Argon2d_Memory_Blocks), "Number of memory blocks must be a power of 2.");
    static_assert(Rx_Argon2d_Iterations - 1 <= std::numeric_limits<uint32_t>::max() - 1, "Number of iterations must be greater than 0.");
    static_assert(Rx_Argon2d_Tag_Length == 0, "Tag length must be 0 for RandomX.");
    static_assert(Rx_Argon2d_Type == 0, "Argon2d type must be 0 for RandomX.");
    static_assert(Rx_Argon2d_Version == 0x13, "Argon2 version must be 0x13 for RandomX.");
    static_assert(Rx_Argon2d_Secret.size() == 0, "Argon2 secret must be empty for RandomX.");
    static_assert(Rx_Argon2d_Data.size() == 0, "Argon2 additional data must be empty for RandomX.");
    static_assert(Rx_Argon2d_Salt.size() >= 8, "It is required by algorithm to use salt with length of at least 8 bytes.");


    // Superscalar Parameters.
    inline constexpr uint32_t Rx_Superscalar_Latency{ 170 }; // Target latency for superscalar program (in cycles of the reference CPU).

    static_assert(Rx_Superscalar_Latency - 1 <= 9999, "Superscalar latency must be in range 1 - 10'000.");

    // Scratchpad Parameters.
    inline constexpr uint32_t Rx_Scratchpad_L1_Size{ 16384 }; // In bytes.
    inline constexpr uint32_t Rx_Scratchpad_L2_Size{ 262144 }; // In bytes.
    inline constexpr uint32_t Rx_Scratchpad_L3_Size{ 2097152 }; // In bytes.

    static_assert(std::has_single_bit(Rx_Scratchpad_L1_Size), "L1 scratchpad size must be a power of 2.");
    static_assert(Rx_Scratchpad_L1_Size >= 64, "L1 scratchpad size must be greater than 64 bytes.");
    static_assert(std::has_single_bit(Rx_Scratchpad_L2_Size), "L2 scratchpad size must be a power of 2.");
    static_assert(Rx_Scratchpad_L2_Size >= Rx_Scratchpad_L1_Size, "L2 scratchpad size must be greater than or equal to L1 scratchpad size.");
    static_assert(std::has_single_bit(Rx_Scratchpad_L3_Size), "L3 scratchpad size must be a power of 2.");
    static_assert(Rx_Scratchpad_L3_Size >= Rx_Scratchpad_L2_Size, "L3 scratchpad size must be greater than or equal to L2 scratchpad size.");

    // Dataset Parameters.
    inline constexpr uint32_t Rx_Dataset_Base_Size{ 2'147'483'648 }; // In bytes (2GB).
    inline constexpr uint32_t Rx_Dataset_Extra_Size{ 33'554'368 }; // In bytes (32MB - 64B).
    inline constexpr uint32_t Rx_Cache_Accesses{ 8 }; // Number of random cache accesses per Dataset item.

    static_assert(Rx_Dataset_Base_Size - 64 <= std::numeric_limits<uint32_t>::max() - 64, "Dataset base size must be in range 64 - 4'294'967'296.");
    static_assert(std::has_single_bit(Rx_Dataset_Base_Size), "Dataset base size must be a power of 2.");
    static_assert(Rx_Dataset_Extra_Size % 64 == 0, "Dataset extra size must be a multiple of 64.");
    static_assert(Rx_Cache_Accesses >= 2 , "Number of cache accesses must be greater-equal than 2.");

    // Program Parameters.
    inline constexpr uint32_t Rx_Program_Size{ 256 }; // The number of instructions in a RandomX program.

    inline constexpr uint32_t Rx_Program_Iterations{ 2048 }; // The number of iterations per program.
    inline constexpr uint32_t Rx_Program_Count{ 8 }; // The number of programs per hash.
    inline constexpr uint32_t Rx_Jump_Bits{ 8 }; // Jump condition mask size in bits.
    inline constexpr uint32_t Rx_Jump_Offset{ 8 }; //Jump condition mask offset in bits.

    static_assert(Rx_Program_Size - 8 <= 32760, "RandomX program size must be in range 8 - 32'768.");
    static_assert(Rx_Program_Iterations > 0, "RandomX program iteration must be greater than 0.");
    static_assert(Rx_Program_Count > 0, "RandomX program count must be greater than 0.");
    static_assert(Rx_Jump_Bits > 0, "Jump bits must be greater than 0.");
    static_assert(Rx_Jump_Bits + Rx_Jump_Offset <= 16, "The sum of jump bits and jump offset must not exceed 16.");

    // Instruction Parameters.
    inline constexpr uint32_t Rx_Freq_Iadd_Rs{ 16 }; // Frequency of IADD_RS instruction.
    inline constexpr uint32_t Rx_Freq_Iadd_M{ 7 }; // Frequency of IADD_M instruction.
    inline constexpr uint32_t Rx_Freq_Isub_R{ 16 }; // Frequency of ISUB_R instruction.
    inline constexpr uint32_t Rx_Freq_Isub_M{ 7 }; // Frequency of ISUB_M instruction.
    inline constexpr uint32_t Rx_Freq_Imul_R{ 16 }; // Frequency of IMUL_R instruction.
    inline constexpr uint32_t Rx_Freq_Imul_M{ 4 }; // Frequency of IMUL_M instruction.
    inline constexpr uint32_t Rx_Freq_Imulh_R{ 4 }; // Frequency of IMULH_R instruction.
    inline constexpr uint32_t Rx_Freq_Imulh_M{ 1 }; // Frequency of IMULH_M instruction.
    inline constexpr uint32_t Rx_Freq_Ismulh_R{ 4 }; // Frequency of ISMULH_R instruction.
    inline constexpr uint32_t Rx_Freq_Ismulh_M{ 1 }; // Frequency of ISMULH_M instruction.
    inline constexpr uint32_t Rx_Freq_Imul_Rcp{ 8 }; // Frequency of IMUL_RCP instruction.
    inline constexpr uint32_t Rx_Freq_Ineg_R{ 2 }; // Frequency of INEG_R instruction.
    inline constexpr uint32_t Rx_Freq_Ixor_R{ 15 }; // Frequency of IXOR_R instruction.
    inline constexpr uint32_t Rx_Freq_Ixor_M{ 5 }; // Frequency of IXOR_M instruction.
    inline constexpr uint32_t Rx_Freq_Iror_R{ 8 }; // Frequency of IROR_R instruction.
    inline constexpr uint32_t Rx_Freq_Irol_R{ 2 }; // Frequency of IROL_R instruction.
    inline constexpr uint32_t Rx_Freq_Iswap_R{ 4 }; // Frequency of ISWAP_R instruction.
    inline constexpr uint32_t Rx_Freq_Fswap_R{ 4 }; // Frequency of FSWAP_R instruction.
    inline constexpr uint32_t Rx_Freq_Fadd_R{ 16 }; // Frequency of FADD_R instruction.
    inline constexpr uint32_t Rx_Freq_Fadd_M{ 5 }; // Frequency of FADD_M instruction.
    inline constexpr uint32_t Rx_Freq_Fsub_R{ 16 }; // Frequency of FSUB_R instruction.
    inline constexpr uint32_t Rx_Freq_Fsub_M{ 5 }; // Frequency of FSUB_M instruction.
    inline constexpr uint32_t Rx_Freq_Fscal_R{ 6 }; // Frequency of FSCAL_R instruction.
    inline constexpr uint32_t Rx_Freq_Fmul_R{ 32 }; // Frequency of FMUL_R instruction.
    inline constexpr uint32_t Rx_Freq_Fdiv_M{ 4 }; // Frequency of FDIV_M instruction.
    inline constexpr uint32_t Rx_Freq_Fsqrt_R{ 6 }; // Frequency of FSQRT_R instruction.
    inline constexpr uint32_t Rx_Freq_Cbranch{ 25 }; // Frequency of CBRANCH instruction.
    inline constexpr uint32_t Rx_Freq_Cfround{ 1 }; // Frequency of CFROUND instruction.
    inline constexpr uint32_t Rx_Freq_Istore{ 16 }; // Frequency of ISTORE instruction.

    template<typename ... Freqs>
    consteval bool freqCheck(Freqs... freqs) {
        static_assert(sizeof...(freqs) == 29, "The number of instruction frequencies must be 29.");

        // Workaround over "fold expression did not evaluate to a constant" error.
        if (((freqs >= 0) && ...) != true) {
            throw "Instruction frequencies must be positive.";
        }
        
        // Workaround over "fold expression did not evaluate to a constant" error.
        if ((freqs + ...) != 256) {
            throw "The sum of instruction frequencies must be 256.";
        }

        return true;
    }

    static_assert(freqCheck(Rx_Freq_Iadd_Rs, Rx_Freq_Iadd_M, Rx_Freq_Isub_R, Rx_Freq_Isub_M, Rx_Freq_Imul_R, Rx_Freq_Imul_M, Rx_Freq_Imulh_R, Rx_Freq_Imulh_M,
                            Rx_Freq_Ismulh_R, Rx_Freq_Ismulh_M, Rx_Freq_Imul_Rcp, Rx_Freq_Ineg_R, Rx_Freq_Ixor_R, Rx_Freq_Ixor_M, Rx_Freq_Iror_R, 
                            Rx_Freq_Irol_R, Rx_Freq_Iswap_R, Rx_Freq_Fswap_R, Rx_Freq_Fadd_R, Rx_Freq_Fadd_M, Rx_Freq_Fsub_R, Rx_Freq_Fsub_M, 
                            Rx_Freq_Fscal_R, Rx_Freq_Fmul_R, Rx_Freq_Fdiv_M, Rx_Freq_Fsqrt_R, Rx_Freq_Cbranch, Rx_Freq_Cfround, Rx_Freq_Istore));
}

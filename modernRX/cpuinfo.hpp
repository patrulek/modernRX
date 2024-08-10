#pragma once

/*
* Wrapper over cpuid intrinsics for detecting CPU capabilities.
* Not a part of RandomX algorithm.
*/

#include <bitset>
#include <intrin.h>

// Based on: https://github.com/cklutz/mcoreinfo/blob/master/sysinfo/CpuCapabilities.cs
// and https://learn.microsoft.com/pl-pl/cpp/intrinsics/cpuid-cpuidex?view=msvc-170
class CPUInfo
{
// Forward declarations.
class CPUInfo_Internal;

public:
    // Returns true if CPU supports AVX512F instructions.
	 [[nodiscard]] static bool AVX512F() { return cpuinfo().f_7_EBX_[16]; };

    // Returns true if CPU supports AVX512VL instructions.
    [[nodiscard]] static bool AVX512VL() { return cpuinfo().f_7_EBX_[31]; };

    // Returns true if CPU supports AVX512DQ instructions.
	 [[nodiscard]] static bool AVX512DQ() { return cpuinfo().f_7_EBX_[17]; };

    // Returns true if CPU supports AES instructions.
    [[nodiscard]] static bool AES() { return cpuinfo().f_1_ECX_[25]; };

    // Returns true if CPU supports hyperthreading.
    [[nodiscard]] static bool HTT() { return cpuinfo().f_1_EDX_[28]; };

private:
    static const CPUInfo_Internal CPU_Rep;

    // Gets CPUInfo_Internal object. If another static object uses CPUInfo methods, but is initialized before CPU_Rep, it will create temporary object.
    [[nodiscard]] static CPUInfo_Internal cpuinfo() {
        return CPU_Rep.initialized ? CPU_Rep : CPUInfo_Internal{};
    };

    // Internal class for retrieving and storing CPU capabilities.
    class CPUInfo_Internal {
    public:
        [[nodiscard]] explicit CPUInfo_Internal() {
            std::array<int, 4> cpui{};

            // Calling __cpuid with 0x0 as the function_id argument
            // gets the number of the highest valid function ID.
            __cpuid(cpui.data(), 0);
            auto nIds_{ cpui[0] };

            for (int i = 0; i <= nIds_; ++i) {
                __cpuidex(cpui.data(), i, 0);
                data_.push_back(cpui);
            }

            // Load bitset with flags for function 0x00000001.
            if (nIds_ >= 1) {
                f_1_ECX_ = data_[1][2];
                f_1_EDX_ = data_[1][3];
            }

            // Load bitset with flags for function 0x00000007.
            if (nIds_ >= 7) {
                f_7_EBX_ = data_[7][1];
            }

            // Calling __cpuid with 0x80000000 as the function_id argument
            // gets the number of the highest valid extended ID.
            cpui.fill(0);
            __cpuid(cpui.data(), 0x80000000);
            nIds_ = cpui[0];

            for (int i = 0x80000000; i <= nIds_; ++i) {
                __cpuidex(cpui.data(), i, 0);
                extdata_.push_back(cpui);
            }

            initialized = true;
        };

        std::bitset<32> f_1_EDX_{ 0 };
        std::bitset<32> f_1_ECX_{ 0 };
        std::bitset<32> f_7_EBX_{ 0 };
        std::vector<std::array<int, 4>> data_{};
        std::vector<std::array<int, 4>> extdata_{};
        bool initialized{ false };
    };
};

// Initialization.
const CPUInfo::CPUInfo_Internal CPUInfo::CPU_Rep{};

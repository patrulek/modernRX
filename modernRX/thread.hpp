#pragma once

#include <bitset>

#include <processthreadsapi.h>

// Returns true if the thread affinity was set successfully.
bool setThreadAffinity(const uint32_t coreid) {
    std::bitset<64> mask;
    mask.set(2 * coreid);

    return SetThreadAffinityMask(GetCurrentThread(), mask.to_ulong());
}
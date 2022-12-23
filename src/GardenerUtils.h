#pragma once
#include <cstdint>

namespace std
{
    class thread;
}


namespace Gardener {

    /* 
     *  Cross - Platform Utilities
     */ 

    // Cross-platform thread affinity setting.
    void SetThreadAffinity(std::thread& thread, uint32_t coreId);

    // Cross-platform getting the CPU core id that the thread is working on.
    uint32_t GetProcessorId();
}
#include "GardenerUtils.h"
#include <thread>

#ifdef __linux__ 
#elif _WIN32
#include <windows.h>
#include <winbase.h>
#else
#endif

namespace Gardener
{
    // ================================================================================================================
    void SetThreadAffinity(
        std::thread& thread, 
        uint32_t     coreId)
    {
#ifdef __linux__
#elif _WIN32
        uint32_t mask = 1 << coreId;
        SetThreadAffinityMask(thread.native_handle(), mask);
#else
#endif
    }

    // ================================================================================================================
    uint32_t GetProcessorId()
    {
#ifdef __linux__
#elif _WIN32
        return static_cast<uint32_t>(GetCurrentProcessorNumber());
#else
#endif
    }
}
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>

#include <boost/intrusive_ptr.hpp>

#include <boost/fiber/all.hpp>

#include "GardenerUtils.h"

/*
*   Used to test whether the multi-threads fiber works. And whether threads have proper affinities to CPU cores.
*/

boost::fibers::mutex fiberMutex;

std::mutex fiberIdMutex;
uint32_t fiberGlobalId = 0;

inline void fiberFunc(uint32_t boundThreadId,
                      uint32_t localFiberId)
{
    for (uint32_t i = 0; i < 3; i++)
    {
        // The mutex is global because the console resource is shared across threads. 
        fiberMutex.lock();
        std::cout << "Thread #" << boundThreadId
                  << ", Fiber #" << localFiberId
                  << ": on CPU " << Gardener::GetProcessorId() << "\n";
        fiberMutex.unlock();
    }    
}

// Periodically checking the core that the thread is hold.
inline void fn(uint32_t customThreadId)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    boost::fibers::fiber fiberArray[3] = {};

    // Spawn three fibers attached on this thread.
    for (uint32_t i = 0; i < 3; i++)
    {
        uint32_t id = 0;
        {
            // RAII style mutex.
            std::lock_guard<std::mutex> idLock(fiberIdMutex);
            id = fiberGlobalId++;
        }

        fiberArray[i] = boost::fibers::fiber(fiberFunc, customThreadId, id);

        // Simulate important work done by the tread by sleeping for a bit...
        std::this_thread::sleep_for(std::chrono::milliseconds(900));
    }

    // Wait for these three fibers
    for (uint32_t i = 0; i < 3; i++)
    {
        fiberArray[i].join();
    }
}

int main() 
{
    unsigned int n = std::thread::hardware_concurrency();
    std::cout << n << " concurrent threads are supported.\n";

    std::thread threadArray[3] = {};

    for (uint32_t i = 0; i < 3; i++)
    {
        threadArray[i] = std::thread(fn, i);
        Gardener::SetThreadAffinity(threadArray[i], i);
    }

    for (auto& t : threadArray) {
        t.join();
    }
    return 0;
}

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>

#include <boost/intrusive_ptr.hpp>

#include "Gardener.h"

/*
*   Example for kicking off a batch of jobs.
*/

/*
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
*/

class CustomTask
{
public:
    CustomTask(uint32_t taskId)
        : m_taskId(taskId)
    {}

    ~CustomTask() {}

    static void MyTaskFunc(CustomTask* pThisTask)
    {
        pThisTask->m_taskRes = pThisTask->m_taskId * pThisTask->m_taskId;
    }

    void PrintTaskRes()
    {
        std::cout << "Task #" << m_taskId << ": "
                  << m_taskRes << std::endl;
    }

    uint64_t m_taskId;

private:
    uint32_t m_taskRes;
};

int main() 
{
    /*
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
    */

    CustomTask* tasks[10];
    for (uint32_t i = 0; i < 10; i++)
    {
        tasks[i] = new CustomTask(i);
    }

    uint32_t runThreadNum = std::thread::hardware_concurrency() / 2;

    Gardener::JobSystem jobSys(runThreadNum);

    for (uint32_t i = 0; i < 10; i++)
    {
        std::cout << typeid (&tasks[i]->MyTaskFunc).name() << std::endl;
        jobSys.AddAJob(&tasks[i]->MyTaskFunc, tasks[i], tasks[i]->m_taskId);
    }

    jobSys.WaitJobsComplete();

    for (uint32_t i = 0; i < 10; i++)
    {
        tasks[i]->PrintTaskRes();
        delete tasks[i];
    }
}

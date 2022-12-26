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
    CustomTask* tasks[10];
    for (uint32_t i = 0; i < 10; i++)
    {
        tasks[i] = new CustomTask(i);
    }

    uint32_t runThreadNum = std::thread::hardware_concurrency() / 2;

    Gardener::JobSystem jobSys(runThreadNum);

    for (uint32_t i = 0; i < 10; i++)
    {
        jobSys.AddAJob(&tasks[i]->MyTaskFunc, tasks[i], tasks[i]->m_taskId);
    }

    jobSys.WaitJobsComplete();

    for (uint32_t i = 0; i < 10; i++)
    {
        tasks[i]->PrintTaskRes();
        delete tasks[i];
    }
}

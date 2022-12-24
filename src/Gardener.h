#pragma once

// Gardener header
#include <cstdint>

namespace boost
{
    namespace fibers
    {
        class mutex;
        class fiber;
    }
}

namespace std
{
    class thread;
}

namespace Gardener
{
    // The function signature for a job's execution function pointer.
    typedef void (*PfnJobEntry)();

    /*
    *   Represents a customizable job. User can provide the function that they want to use through the
    *   SetEntryFunc(...). The job can be spawned through SpawnJob(...) or SpawnJobs(...) interfaces specified in the
    *   JobSystem class.
    * 
    *   The user should notice that they can and should use the Boost functionalities in the provided function pointer.
    *   And the provided function pointer would be used to create a Boost::fiber after spawning.
    *   Examples can be found in the Gardener's example folder.
    */
    class Job
    {
    public:
        Job();
        ~Job();

        void SetEntryFunc(const PfnJobEntry& func) { m_pfnEntryPoint = func; }

    protected:
        PfnJobEntry m_pfnEntryPoint;
    };

    // Job queue maintains a ring buffer used to store job pointers.
    class JobQueue
    {
    public:
        JobQueue(uint32_t queueSize);
        ~JobQueue();

        void AcquireAJob();
        void SendAJob();
        void SendJobs();

        boost::fibers::mutex* m_pQueueAccessMutex;
    private:
        
        uint32_t m_jobCnt;
        Job**    m_ppHeader;

        uint32_t m_queueSize;
        Job**    m_ppJobsRingBuffer;
    };

    // Each worker is bound to a thread and executes jobs in the job queue.
    class Worker
    {
    public:
        Worker();
        ~Worker();

        void StartThread();

        // 
        void WorkerLoop();

    private:
        
        std::thread*          m_pThread;
        boost::fibers::fiber* m_pFibers;
        uint32_t              m_fiberNum;
    };

    // Job system gets jobs' function pointers from the outside, which is the main interface.
    class JobSystem
    {
    public:
        JobSystem(uint32_t threadNum);
        ~JobSystem();

        // Add jobs into the job queue managed by the job system.
        void AddAJob(PfnJobEntry jobEntry);
        void AddJobs(PfnJobEntry* const pJobsEntries, uint32_t jobCnt);

        // The srcJobEntry would execute after the desJobEntry finishes.
        void AddDepJobs(PfnJobEntry srcJobEntry, PfnJobEntry dstJobEntry);

    private:
        JobQueue* m_pJobQueue;
        Worker*   m_pWorkers;
        uint32_t  m_workersCnt;
    };
}
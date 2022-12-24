#pragma once

// Gardener header
#include <cstdint>
#include <unordered_map>
#include <queue>
#include <thread>
#include <mutex>

namespace boost
{
    namespace fibers
    {
        class mutex;
        class fiber;
    }
}

namespace Gardener
{
    // The function signature for a job's execution function pointer.
    typedef void (*PfnJobEntry)();

    enum JobStatusEnum
    {
        IN_QUEUE, IN_WORKER
    };

    /*
    *   Represents a customizable job. The job can be spawned through AddAJob(...), AddJobs(...) or AddDepJobs(...)
    *   interfaces specified in the JobSystem class.
    * 
    *   The user should notice that they can and should use the Boost functionalities in the provided function pointer.
    *   And the provided function pointer would be executed in a Boost::fiber after spawning.
    *   Examples can be found in the Gardener's example folder.
    */
    class Job
    {
    public:
        explicit Job(const PfnJobEntry& func, uint64_t jobId);
        ~Job();

        void SetEntryFunc(const PfnJobEntry& func) { m_pfnEntryPoint = func; }
        PfnJobEntry GetEntryFunc() const { return m_pfnEntryPoint; }

        uint64_t GetId() const { return m_jobId; }

    private:
        PfnJobEntry           m_pfnEntryPoint;
        uint64_t              m_jobId;
        JobStatusEnum         m_status;
        boost::fibers::fiber* m_pFiber;
    };

    // Job queue maintains a queue to store job pointers.
    class JobQueue
    {
    public:
        JobQueue();
        ~JobQueue();

        // This function is called by a worker to get work.
        Job* SendOutAJob();

        // This function is called by the job system to put a job into the queue.
        void SendInAJob(Job* pJob);

        void SendJobs();

    private:
        boost::fibers::mutex* m_pQueueAccessMutex;
        std::queue<Job*> m_queue;

    };

    // Each worker is bound to a thread and executes jobs in the job queue.
    class Worker
    {
    public:
        explicit Worker(JobQueue* const pJobQueue, uint32_t affinityCoreId);
        ~Worker();

        // Spawn a thread and let the thread execute the WorkerLoop().
        void StartWork();

        // Block the caller thread until the works are done and safe to be deleted.
        // This function would delete the current thread object.
        void StopWork();

    private:
        // Executed in the worker thread, this func is responsible for constantly grabbing the works from the job queue.
        static void WorkerLoop(Worker* pWorker);
        
        std::thread*          m_pThread;
        uint32_t              m_coreIdAffinity;

        std::unordered_map<uint64_t, boost::fibers::fiber*> m_fiberList;

        // The stop signal would be shared between the main thread and the worker thread. When the StopWork() is called
        // in the main thread, it would set the stop signal to true. Meanwhile in the worker thread's WorkerLoop(), the
        // worker thread would peroidically check whether it needs to stop working on new jobs in the job queue.
        bool                  m_stopSignal;
        std::mutex            m_stopSignalMutex;

        JobQueue* const       m_pJobQueue;
    };

    /*  
     * Job system gets jobs' function pointers from the outside, which is the main interface. The dev can put custom
     * job entry functions into AddAJob(...), AddJobs(...) or AddDepJobs(...) and get their jobs handles/Ids back.
     * 
     * This implies that all the jobs instances are managed internally by the JobSystem.
     * 
     * For syn, you can either use the AddDepJobs(...) or use the returned jobs' handles to access the 
     * boost::fibers::fiber in your jobs' function, which will be executed in a fiber. And keeping in mind that the job
     * won't be executed immediately, so you need to take care the checking logic in your job's entry point function.
     * 
     * It is dev's responsibility to provide the address/memory for storing the returned job handles/Ids.
     * 
     * Currently, it just set core affinity for the first threadNum cores.
     */ 
    class JobSystem
    {
    public:
        JobSystem(uint32_t threadNum);
        ~JobSystem();

        // Add jobs into the job queue managed by the job system.
        void AddAJob(PfnJobEntry jobEntry, uint64_t& jobId);
        void AddJobs(PfnJobEntry* const pJobsEntries, uint32_t jobCnt, uint64_t* jobIds);

        // The srcJobEntry would execute after the desJobEntry finishes.
        void AddDepJobs(PfnJobEntry srcJobEntry, PfnJobEntry dstJobEntry, uint64_t* jobIds);
        
        // Called by the worker when it finishes the job.
        void JobFinishes(const uint64_t jobId);

        // If the job finishes, the func would return nullptr.
        Job* GetJobPtrFromId(uint64_t jobId);

    private:
        JobQueue* m_pJobQueue;
        Worker**  m_ppWorkers;
        uint32_t  m_workersCnt;

        std::unordered_map<uint64_t, Job*>*  m_pJobsHandleTable; // The handle table stores all jobs instances.
        uint64_t                             m_servedJobsCnt;    // Will be used to assign a unique ID for the input
                                                                 // job.
        std::mutex                           m_handleTblMutex;   // The job handle table maybe accessd by multiple 
                                                                 // threads. E.g. The main thread adds jobs to the
                                                                 // table. The worker threads remove jobs from the
                                                                 // handle table.
    };
}
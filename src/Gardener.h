#pragma once

// Gardener header
#include <boost/fiber/all.hpp>
#include <cstdint>
#include <unordered_map>
#include <queue>
#include <thread>
#include <mutex>
#include <list>

// TODO: The fiber mutex are spinning-lock. We may need to redesign.
// Previously, I thought that is a yield.
// S postfix on data name for shared resources.
// T postfix on function name with thread syn.
// F postfix on function name with fiber syn (non-preemptive).
//
// Three priorities of fiber works and their locks usage: Low - If-Try-Lock; Mid: While-Try-Lock; High: Lock.
//
namespace Gardener
{
    class JobSystem;

    // The function signature for a job's execution function pointer.
    typedef void (*PfnJobEntry)(void*);

    /*
    *   Represents a customizable job. The job can be spawned through AddAJob(...), AddJobs(...) or AddDepJobs(...)
    *   interfaces specified in the JobSystem class.
    * 
    *   The user should be aware of the Boost functionalities in the provided function pointer.
    *   And the provided function pointer would be executed in a Boost::fiber after spawning.
    *   Examples can be found in the Gardener's example folder.
    * 
    *   The job system is responsible for managing the the life time of a job.
    */
    class Job
    {
    public:
        explicit Job(const PfnJobEntry& func, void* pCustomTask, uint64_t jobId);
        ~Job();

        PfnJobEntry GetEntryFunc() const { return m_pfnEntryPoint; }
        uint64_t GetId() const { return m_jobId; }
        void* GetCustomTask() const { return m_pCustomTask; }

    private:
        PfnJobEntry m_pfnEntryPoint;
        uint64_t    m_jobId;
        void*       m_pCustomTask;
    };

    // Job queue maintains a queue to store job pointers sent from the job system and would sent job pointers to the
    // worker that request jobs.
    //
    // It also maintains the syn in its interfaces, so we don't need to manage the syn in the job system object and let
    // workers access the job queue object indirectly through the job system.
    class JobQueue
    {
    public:
        JobQueue();
        ~JobQueue();

        // This function is called by a worker to get work.
        Job* SendOutAJobF();

        // This function is called by the job system to put a job into the queue. It may blocks the caller thread 
        void SendInAJobF(Job* pJob);

    private:
        boost::fibers::mutex m_queueAccessMutex;
        std::queue<Job*>     m_queueS;           // The queue of jobs that are waiting for workers.

    };

    // Each worker is bound to a thread and executes jobs in the job queue.
    class Worker
    {
    public:
        Worker(JobQueue* const pJobQueue, JobSystem* const pJobSys, uint32_t affinityCoreId);
        ~Worker();

        // Spawn a thread and let the thread execute the WorkerLoop().
        void StartWork();

        // Block the caller thread until the works are done and release all the heap memory managed by the worker.
        // This function is normally called by the main thread in the worker's destructor. It signals the worker thread
        // to stop. Then, it would wait the worker thread to finish. Then it deletes the current thread object and
        // terminate all running fibers.
        void StopWorkFT();

    private:
        // Executed in the worker thread, this func is responsible for constantly grabbing the works from the job queue.
        static void WorkerLoopFT(Worker* pWorker);

        // The wrapper function is used to spawn a fiber. In this func, it would do preamble and postamble 
        static void EntryFuncWrapperF(Worker* pWorker, Job* pJob);
        
        std::thread*          m_pThread;
        uint32_t              m_coreIdAffinity;

        // The fiber list is shared between the main thread and the worker thread. It won't have the contension because
        // the main thread only access it after the worker thread joins. So, no need for the protection.
        // 
        // The programmer must ensure that the destructor is never executed while the fiber is still fiber::joinable(). 
        // Even if you know that the fiber has completed, you must still call either fiber::join() or fiber::detach() 
        // before destroying the fiber object.
        std::unordered_map<uint64_t, boost::fibers::fiber*> m_fiberListS;

        // Used to free fibers in the fiberList during each workerLoop. This queue is shared among all the fibers in
        // in the worker and we need to give it protection. The fiber mutex is collabrative, which means if a fiber is
        // unable to own the mutex, it just gives control back to the scheduler.
        std::queue<uint64_t> m_retiredJobQueueS; 
        boost::fibers::mutex m_retiredJobQueueMutex;

        // The stop signal would be shared between the main thread and the worker thread. When the StopWork() is called
        // in the main thread, it would set the stop signal to true. Meanwhile in the worker thread's WorkerLoop(), the
        // worker thread would peroidically check whether it needs to stop working on new jobs in the job queue. Write
        // at very beginning, only read afterward. No need for protection.
        bool                  m_stopSignalS;

        JobQueue* const       m_pJobQueueS; // Reference to the job queue of getting jobs. Shared between fibers.
                                            // Read only, no need for protection.
        JobSystem* const      m_pJobSysS;   // Reference to the job system to informing the system that a job has done.
                                            // Shared between fibers. Read only, no need for protection.
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

        // Create and add jobs into the job queue managed by the job system.
        template<typename FT, typename T>
        inline void AddAJob(FT jobEntry, T* pTask, uint64_t& jobId)
        {
            // Check whether the user provids a proper job entry function pointer.
            typedef void (*PfnJobEntryInput)(T*);
            static_assert(std::is_same<FT, PfnJobEntryInput>::value, 
                          "The input jobEntry function pointer is incompatiable with the input pTask pointer.");
            AddAJobInternalF(static_cast<void*>(jobEntry), static_cast<void*>(pTask), jobId);
        }

        // Called by the worker when it finishes the job.
        void JobFinishesF(const uint64_t jobId);

        // Wait for all submitted jobs done in the main thread.
        void WaitJobsComplete();

    private:
        void AddAJobInternalF(void* jobEntry, void* pTask, uint64_t& jobId);

        JobQueue*          m_pJobQueueS;
        std::list<Worker*> m_pWorkers;

        std::unordered_map<uint64_t, Job*>*  m_pJobsHandleTableS; // The handle table stores all jobs instances.
        uint64_t                             m_servedJobsCnt;     // Will be used to assign a unique ID for the input
                                                                  // job. Shared Resource.
        boost::fibers::mutex                 m_handleTblMutex;    // The job handle table maybe accessd by multiple 
                                                                  // threads. E.g. The main thread adds jobs to the
                                                                  // table. The worker threads remove jobs from the
                                                                  // handle table or add jobs from the fiber. Shared
                                                                  // resource.
        uint64_t                             m_doneJobsCntS;      // It needs to share the handle table mutex.
    };
}
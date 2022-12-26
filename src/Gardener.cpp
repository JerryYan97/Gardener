#include "Gardener.h"
#include "GardenerUtils.h"

namespace Gardener
{
    // ================================================================================================================
    JobSystem::JobSystem(
        uint32_t threadNum) :
        m_doneJobsCntS(0),
        m_servedJobsCnt(0)
    {
        m_pJobQueueS = new JobQueue();
        m_pJobsHandleTableS = new std::unordered_map<uint64_t, Job*>();

        // Creating and kicking off workers
        for (uint32_t i = 0; i < threadNum; i++)
        {
            m_pWorkers.push_back(new Worker(m_pJobQueueS, this, i));
            m_pWorkers.back()->StartWork();
        }
    }

    // ================================================================================================================
    JobSystem::~JobSystem()
    {
        for (Worker* itr : m_pWorkers)
        {
            delete itr;
        }

        if (m_pJobQueueS != nullptr)
        {
            delete m_pJobQueueS;
        }

        if (m_pJobsHandleTableS != nullptr)
        {
            delete m_pJobsHandleTableS;
        }
    }

    // ================================================================================================================
    void JobSystem::AddAJobInternalF(
        void* jobEntry,
        void* pTask,
        uint64_t& jobId)
    {
        Job* pJob = nullptr;
        {
            m_handleTblMutex.lock();
            PfnJobEntry castEntry = static_cast<PfnJobEntry>(jobEntry);
            pJob = new Job(castEntry, pTask, m_servedJobsCnt);
            m_pJobsHandleTableS->insert({ m_servedJobsCnt, pJob });
            m_handleTblMutex.unlock();
        }

        m_pJobQueueS->SendInAJobF(pJob);

        jobId = m_servedJobsCnt++;
    }
    
    // ================================================================================================================
    void JobSystem::JobFinishesF(const uint64_t jobId)
    {
        m_handleTblMutex.lock();
        m_pJobsHandleTableS->erase(jobId);
        m_doneJobsCntS++;
        m_handleTblMutex.unlock();
    }

    // ================================================================================================================
    void JobSystem::WaitJobsComplete()
    {
        while (1)
        {
            if (m_doneJobsCntS == m_servedJobsCnt)
            {
                break;
            }
        }
    }

    // ================================================================================================================
    Worker::Worker(
        JobQueue*  const pJobQueue,
        JobSystem* const pJobSys,
        uint32_t         affinityCoreId)
        : m_pJobQueueS(pJobQueue),
          m_coreIdAffinity(affinityCoreId),
          m_stopSignalS(false),
          m_pThread(nullptr),
          m_pJobSysS(pJobSys)
    {}

    // ================================================================================================================
    void Worker::WorkerLoopFT(
        Worker* pWorker)
    {
        while (1)
        {
            // Check whether the worker gets a stop signal. Jump out of the loop if there is one.
            {
                std::lock_guard<std::mutex> stopSigLock(pWorker->m_stopSignalMutex);
                if (pWorker->m_stopSignalS)
                {
                    break;
                }
            }

            // Retire all the jobs in the retire queue. 
            // (Free all fibers with the job ids specified in the retired queue)
            pWorker->m_retiredJobQueueMutex.lock();
            bool isLock = true;
            while (pWorker->m_retiredJobQueueS.empty() != true)
            {
                uint64_t id = pWorker->m_retiredJobQueueS.front();
                pWorker->m_retiredJobQueueS.pop();
                pWorker->m_retiredJobQueueMutex.unlock();
                isLock = false;

                boost::fibers::fiber* pFiber = pWorker->m_fiberListS.at(id);
                pFiber->join();
                delete pFiber;
                pWorker->m_fiberListS.erase(id);

                // Tell the job system to delete the job from the memory.
                pWorker->m_pJobSysS->JobFinishesF(id);
            }

            if (isLock)
            {
                pWorker->m_retiredJobQueueMutex.unlock();
            }
            
            // Get a job from the jobQueue. If there is one, then we create a fiber for it and execute it. If not, we
            // come back to the spinning loop to check the queue until there is a stop signal or there is a job.
            Job* pJob = pWorker->m_pJobQueueS->SendOutAJobF();
            if (pJob != nullptr)
            {
                // If there is a job that we can work on.
                pWorker->m_fiberListS.insert({ pJob->GetId(),
                                              new boost::fibers::fiber(&Worker::EntryFuncWrapperF, pWorker, pJob)});
            }

            boost::this_fiber::yield();
            std::this_thread::yield();
        }
    }

    // ================================================================================================================
    void Worker::EntryFuncWrapperF(
        Worker* pWorker,
        Job* pJob)
    {
        // Execute the custom job
        pJob->GetEntryFunc()(pJob->GetCustomTask());

        // Put the finished job ID into the Retirement queue. The main fiber of this thread will loop through this
        // queue to signal the job system to delete the job from the jobs table and increase the counter for the
        // overall synchronization.
        pWorker->m_retiredJobQueueMutex.lock();
        pWorker->m_retiredJobQueueS.push(pJob->GetId());
        pWorker->m_retiredJobQueueMutex.unlock();
    }

    // ================================================================================================================
    Worker::~Worker()
    {
        StopWorkFT();
    }

    // ================================================================================================================
    void Worker::StartWork()
    {
        // Spawn a thread with worker loop. Note that this func is called in the main thread.
        m_pThread = new std::thread(&Worker::WorkerLoopFT, this);
        SetThreadAffinity(*m_pThread, m_coreIdAffinity);
    }

    // ================================================================================================================
    void Worker::StopWorkFT()
    {
        // Block the caller thread to wait for the worker thread.
        if (m_pThread != nullptr)
        {
            {
                std::lock_guard<std::mutex> stopSigLock(m_stopSignalMutex);
                m_stopSignalS = true;
            }

            // Wait until the worker thread stop working. We can delete all the fiber objects in the worker.
            m_pThread->join();
            for (auto& itr : m_fiberListS)
            {
                itr.second->join();
                delete itr.second;
            }
            delete m_pThread;
        }
    }

    // ================================================================================================================
    JobQueue::JobQueue()
    {}

    // ================================================================================================================
    JobQueue::~JobQueue()
    {}
    
    // ================================================================================================================
    void JobQueue::SendInAJobF(
        Job* pJob)
    {
        // The main thread is implicitly a main fiber even though we didn't explicitly launch one. It would just
        // spinning if the mutex is owned by others.
        m_queueAccessMutex.lock();
        m_queueS.push(pJob);
        m_queueAccessMutex.unlock();
    }

    // ================================================================================================================
    Job* JobQueue::SendOutAJobF()
    {
        // It the fiber caller is blocked here, it just jumps back to the scheduler of the attached thread.
        m_queueAccessMutex.lock();
        Job* pRes = nullptr;
        
        if (m_queueS.empty() == false)
        {
            pRes = m_queueS.front();
            m_queueS.pop();
        }

        m_queueAccessMutex.unlock();
        return pRes;
    }

    // ================================================================================================================
    Job::Job(
        const PfnJobEntry& func,
        void* pCustomTask,
        uint64_t jobId)
        : m_pfnEntryPoint(func),
          m_jobId(jobId),
          m_pCustomTask(pCustomTask)
    {}

    // ================================================================================================================
    Job::~Job()
    {}
}
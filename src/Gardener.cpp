#include "Gardener.h"
#include "GardenerUtils.h"
#include <boost/fiber/all.hpp>
#include <type_traits>
namespace Gardener
{
    // ================================================================================================================
    JobSystem::JobSystem(
        uint32_t threadNum) :
        m_workersCnt(threadNum),
        m_doneJobsCnt(0)
    {
        m_pJobQueue = new JobQueue();
        m_pJobsHandleTable = new std::unordered_map<uint64_t, Job*>();

        // Creating and kicking off workers
        for (uint32_t i = 0; i < m_workersCnt; i++)
        {
            m_pWorkers.push_back(new Worker(m_pJobQueue, i));
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

        if (m_pJobQueue != nullptr)
        {
            delete m_pJobQueue;
        }

        if (m_pJobsHandleTable != nullptr)
        {
            delete m_pJobsHandleTable;
        }
    }

    // ================================================================================================================
    Job* JobSystem::GetJobPtrFromId(
        uint64_t jobId)
    {
        // Handle table is a shared resrouce among threads, which needs to be syned.
        std::lock_guard<std::mutex> lock(m_handleTblMutex);
        if (m_pJobsHandleTable->find(jobId) != m_pJobsHandleTable->end())
        {
            // The jobId is in the handle table.
            return m_pJobsHandleTable->at(jobId);
        }
        else
        {
            // The jobId is not in the handle table.
            return nullptr;
        }
    }

    // ================================================================================================================
    void JobSystem::AddAJobInternal(
        void* jobEntry,
        void* pTask,
        uint64_t& jobId)
    {
        Job* pJob = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_handleTblMutex);
            PfnJobEntry castEntry = static_cast<PfnJobEntry>(jobEntry);
            pJob = new Job(castEntry, pTask, m_servedJobsCnt);
            m_pJobsHandleTable->insert({ m_servedJobsCnt, pJob });
        }

        m_pJobQueue->SendInAJob(pJob);

        jobId = m_servedJobsCnt++;
    }
    
    // ================================================================================================================
    void JobSystem::JobFinishes(const uint64_t jobId)
    {
        std::lock_guard<std::mutex> lock(m_handleTblMutex);
        m_pJobsHandleTable->erase(jobId);
    }

    // ================================================================================================================
    void JobSystem::WaitJobsComplete()
    {
        while (1)
        {
            std::lock_guard<std::mutex> lock(m_doneJobsCntMutex);
            if (m_doneJobsCnt == m_servedJobsCnt)
            {
                break;
            }
        }
    }

    void JobSystem::AJobDone()
    {
        while (m_doneJobsCntMutex.try_lock() == false)
        {
            boost::this_fiber::yield();
        }

        m_doneJobsCnt++;

        m_doneJobsCntMutex.unlock();
    }

    // ================================================================================================================
    Worker::Worker(
        JobQueue* pJobQueue,
        uint32_t affinityCoreId)
        : m_pJobQueue(pJobQueue),
          m_coreIdAffinity(affinityCoreId),
          m_stopSignal(false),
          m_pThread(nullptr)
    {}

    // ================================================================================================================
    void Worker::WorkerLoop(
        Worker* pWorker)
    {
        while (1)
        {
            // Check whether the worker gets a stop signal. Jump out of the loop if there is one.
            {
                std::lock_guard<std::mutex> stopSigLock(pWorker->m_stopSignalMutex);
                if (pWorker->m_stopSignal)
                {
                    break;
                }
            }

            // Retire all the jobs in the retire queue. 
            // (Free all fibers with the job ids specified in the retired queue)
            /*
            if (pWorker->m_retiredJobQueueMutex.try_lock())
            {
                
            }
            */

            while (pWorker->m_retiredJobQueue.empty() != true)
            {
                uint64_t id = pWorker->m_retiredJobQueue.front();
                pWorker->m_retiredJobQueue.pop();

                delete pWorker->m_fiberList.at(id);
                pWorker->m_fiberList.erase(id);

                // Tell the job system to delete the job from the memory.
            }
            
            // Get a job from the jobQueue. If there is one, then we create a fiber for it and execute it. If not, we
            // come back to the spinning loop to check the queue until there is a stop signal or there is a job.
            Job* pJob = pWorker->m_pJobQueue->SendOutAJob();
            if (pJob != nullptr)
            {
                // If there is a job that we can work on.
                pWorker->m_fiberList.insert({ pJob->GetId(), 
                                              new boost::fibers::fiber(&Worker::EntryFuncWrapper, pWorker, pJob)});
            }
        }
    }

    // ================================================================================================================
    void Worker::EntryFuncWrapper(
        Worker* pWorker,
        Job* pJob)
    {
        // Execute the custom job
        pJob->GetEntryFunc()(pJob->GetCustomTask());

        // Put the finished job ID into the Retirement queue. The main fiber of this thread will loop through this
        // queue to signal the job system to delete the job from the jobs table and increase the counter for the
        // overall synchronization.
        pWorker->m_retiredJobQueue.push(pJob->GetId());
    }

    // ================================================================================================================
    Worker::~Worker()
    {
        StopWork();
    }

    // ================================================================================================================
    void Worker::StartWork()
    {
        // Spawn a thread with worker loop. Note that this func is called in the main thread.
        m_pThread = new std::thread(&Worker::WorkerLoop, this);
        SetThreadAffinity(*m_pThread, m_coreIdAffinity);
    }

    // ================================================================================================================
    void Worker::StopWork()
    {
        // Block the caller thread to wait for the worker thread.
        if (m_pThread != nullptr)
        {
            {
                std::lock_guard<std::mutex> stopSigLock(m_stopSignalMutex);
                m_stopSignal = true;
            }

            for (auto& itr : m_fiberList)
            {
                itr.second->join();
                delete itr.second;
            }
            m_pThread->join();
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
    void JobQueue::SendInAJob(
        Job* pJob)
    {
        std::lock_guard<std::mutex> lock(m_queueAccessMutex);
        m_queue.push(pJob);
    }

    // ================================================================================================================
    Job* JobQueue::SendOutAJob()
    {
        // This function would be called by fibers from different threads. So, we need to syn them in a collabrative 
        // way.
        while (m_queueAccessMutex.try_lock() == false)
        {
            boost::this_fiber::yield();
        }
        
        if (m_queue.empty())
        {
            m_queueAccessMutex.unlock();
            return nullptr;
        }
        else
        {
            Job* pJob = m_queue.front();
            m_queue.pop();
            m_queueAccessMutex.unlock();
            return pJob;
        }
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
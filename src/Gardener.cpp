#include "Gardener.h"
#include "GardenerUtils.h"
#include <boost/fiber/all.hpp>

namespace Gardener
{
    // ================================================================================================================
    JobSystem::JobSystem(
        uint32_t threadNum) :
        m_workersCnt(threadNum),
        m_ppWorkers(nullptr)
    {
        m_pJobQueue = new JobQueue();
        m_pJobsHandleTable = new std::unordered_map<uint64_t, Job*>();

        // Creating and kicking off workers
        for (uint32_t i = 0; i < m_workersCnt; i++)
        {
            m_ppWorkers[i] = new Worker(m_pJobQueue, i);
            m_ppWorkers[i]->StartWork();
        }
    }

    // ================================================================================================================
    JobSystem::~JobSystem()
    {
        for (uint32_t i = 0; i < m_workersCnt; i++)
        {
            m_ppWorkers[i]->StopWork();
            delete m_ppWorkers[i];
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
    void JobSystem::AddAJob(
        PfnJobEntry jobEntry,
        uint64_t& jobId)
    {
        Job* pJob = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_handleTblMutex);
            pJob = new Job(jobEntry, m_servedJobsCnt);
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
    Worker::Worker(
        JobQueue* const pJobQueue,
        uint32_t affinityCoreId)
        : m_pJobQueue(pJobQueue),
          m_coreIdAffinity(affinityCoreId)
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

            // Get a job from the jobQueue. If there is one, then we create a fiber for it and execute it. If not, we
            // come back to the spin-lock to check the queue until there is a stop signal or there is a job.
            Job* pJob = pWorker->m_pJobQueue->SendOutAJob();
            if (pJob != nullptr)
            {
                // If there is a job that we can work on. TODO: We may don't need the pWorker input.
                pWorker->m_fiberList.insert({ pJob->GetId(), 
                                              new boost::fibers::fiber(&Worker::EntryFuncWrapper, pWorker, pJob)});
            }
        }
    }

    // ================================================================================================================
    void Worker::EntryFuncWrapper(
        Worker* pWorker, 
        Job* pJob)
    {}

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
            m_pThread->join();
            for (auto& itr : m_fiberList)
            {
                delete itr.second;
            }
            delete m_pThread;
        }
    }

    // ================================================================================================================
    JobQueue::JobQueue()
    {
        m_pQueueAccessMutex = new boost::fibers::mutex();
    }

    // ================================================================================================================
    JobQueue::~JobQueue()
    {
        delete m_pQueueAccessMutex;
    }
    
    // ================================================================================================================
    void JobQueue::SendInAJob(Job* pJob)
    {}

    // ================================================================================================================
    Job* JobQueue::SendOutAJob()
    {
        return nullptr;
    }

    // ================================================================================================================
    Job::Job(
        const PfnJobEntry& func, 
        uint64_t jobId)
        : m_pfnEntryPoint(func),
          m_jobId(jobId)
    {}

    // ================================================================================================================
    Job::~Job()
    {}
}
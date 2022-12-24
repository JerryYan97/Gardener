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

        // Kick off workers
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
        Job* pJob = new Job(jobEntry, m_servedJobsCnt);
        pJob->SetEntryFunc(jobEntry);
        {
            std::lock_guard<std::mutex> lock(m_handleTblMutex);
            m_pJobsHandleTable->insert({ m_servedJobsCnt, pJob });
        }

        m_pJobQueue->SendInAJob(pJob);

        jobId = m_servedJobsCnt++;
    }

    // ================================================================================================================
    void JobSystem::JobFinishes(const uint64_t jobId)
    {
        {
            std::lock_guard<std::mutex> lock(m_handleTblMutex);
            m_pJobsHandleTable->erase(jobId);
        }
    }

    // ================================================================================================================
    Worker::Worker(
        JobQueue* const pJobQueue,
        uint32_t affinityCoreId)
        : m_pJobQueue(pJobQueue),
          m_coreIdAffinity(affinityCoreId)
    {}

    // ================================================================================================================
    void Worker::WorkerLoop(Worker* pWorker)
    {
        while (1)
        {
            // Check whether the worker gets a stop signal.
            {
                std::lock_guard<std::mutex> stopSigLock(pWorker->m_stopSignalMutex);
                if (pWorker->m_stopSignal)
                {
                    break;
                }
            }

            Job* pJob = pWorker->m_pJobQueue->SendOutAJob();
            if (pJob != nullptr)
            {
                // If there is a job that we can work on.
                pWorker->m_fiberList.insert({ pJob->GetId(), new boost::fibers::fiber(pJob->GetEntryFunc()) });
            }
            
        }
    }

    // ================================================================================================================
    Worker::~Worker()
    {}

    void func()
    {}

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
        m_pThread->join();
        delete m_pThread;
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
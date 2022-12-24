#include <thread>
#include "Gardener.h"
#include <boost/fiber/all.hpp>

namespace Gardener
{
    // ================================================================================================================
    JobSystem::JobSystem(
        uint32_t threadNum) :
        m_workersCnt(threadNum),
        m_pWorkers(nullptr),
        m_pJobQueue(nullptr)
    {
        m_pWorkers = new Worker[m_workersCnt];

        // Set the queue size to 128, which is just a magic number.
        m_pJobQueue = new JobQueue(128);
    }

    // ================================================================================================================
    JobSystem::~JobSystem()
    {
        for (uint32_t i = 0; i < m_workersCnt; i++)
        {
            delete &m_pWorkers[i];
        }

        if (m_pJobQueue != nullptr)
        {
            delete m_pJobQueue;
        }
    }

    // ================================================================================================================
    Worker::Worker()
    {}

    // ================================================================================================================
    Worker::~Worker()
    {}

    // ================================================================================================================
    JobQueue::JobQueue(uint32_t queueSize)
        : m_ppJobsRingBuffer(nullptr),
          m_jobCnt(0),
          m_ppHeader(nullptr),
          m_queueSize(queueSize)
    {
        m_ppJobsRingBuffer = static_cast<Job**>(malloc(sizeof(Job*) * m_queueSize));
        m_ppHeader         = m_ppJobsRingBuffer;
    }

    // ================================================================================================================
    JobQueue::~JobQueue()
    {
        delete m_ppJobsRingBuffer;
        m_ppHeader = nullptr;
    }

    // ================================================================================================================
    Job::Job()
    {}

    // ================================================================================================================
    Job::~Job()
    {}
}
// Gardener header
#include <cstdint>

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
        Job() = default;
        ~Job() {}

        void SetEntryFunc(const PfnJobEntry& func) { m_pfnEntryPoint = func; }

    protected:
        PfnJobEntry m_pfnEntryPoint;
    };

    class JobQueue
    {
    public:
        JobQueue() = default;
        ~JobQueue() {}
    };

    class JobSystem
    {
    public:
        JobSystem() = default;
        ~JobSystem() {}

        void SpawnJob(const Job& job);
        void SpawnJobs(const Job* const pJobs, uint32_t jobCnt);
    private:

        uint32_t m_jobCnt;
    };
}
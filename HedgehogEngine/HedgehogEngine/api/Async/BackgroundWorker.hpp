#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <stop_token>
#include <thread>
#include <utility>

namespace HedgehogEngine
{
    // One thread that runs jobs one at a time, in the order they were submitted (for loading assets
    // off the frame). A job takes the worker's stop token, so a long one can give up once Stop is
    // asked. Stop (and the destructor) skips the jobs still queued, lets the running one finish and
    // joins the thread.
    class BackgroundWorker
    {
    public:
        using JobId = uint64_t; // from 1, in submission order
        using Job   = std::function<void(std::stop_token)>;

        HEDGEHOG_ENGINE_API BackgroundWorker();
        HEDGEHOG_ENGINE_API ~BackgroundWorker();

        BackgroundWorker(const BackgroundWorker&)            = delete;
        BackgroundWorker& operator=(const BackgroundWorker&) = delete;

        // Queues job. A job submitted after Stop is skipped at once (IsDone is already true).
        HEDGEHOG_ENGINE_API JobId Submit(Job job);
        // Whether the job has run or was skipped.
        [[nodiscard]] HEDGEHOG_ENGINE_API bool IsDone(JobId id) const;
        // Blocks until IsDone(id).
        HEDGEHOG_ENGINE_API void Wait(JobId id) const;
        // Skips the queued jobs, asks the running one to stop and joins the thread; safe twice.
        HEDGEHOG_ENGINE_API void Stop();
        [[nodiscard]] HEDGEHOG_ENGINE_API bool IsStopped() const;

    private:
        void Run(std::stop_token stop);

        mutable std::mutex                  m_Mutex;
        mutable std::condition_variable_any m_Changed;
        std::deque<std::pair<JobId, Job>>   m_Queue;
        JobId                               m_LastSubmitted = 0;
        JobId                               m_LastDone      = 0; // jobs run in order, so every id up to it is done
        bool                                m_Stopped       = false;
        bool                                m_Joined        = false;
        std::jthread                        m_Thread;           // last, so it starts after the rest is built
    };
}

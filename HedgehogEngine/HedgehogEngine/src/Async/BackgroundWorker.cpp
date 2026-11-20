#include "HedgehogEngine/api/Async/BackgroundWorker.hpp"

#include "Logger/api/Logger.hpp"

#include <exception>
#include <utility>

namespace HedgehogEngine
{
    BackgroundWorker::BackgroundWorker()
        : m_Thread([this](std::stop_token stop) { Run(stop); })
    {
    }

    BackgroundWorker::~BackgroundWorker()
    {
        Stop();
    }

    BackgroundWorker::JobId BackgroundWorker::Submit(Job job)
    {
        JobId id = 0;
        {
            std::lock_guard lock(m_Mutex);
            id = ++m_LastSubmitted;
            if (m_Stopped)
            {
                // Skipped: once the thread is joined nothing runs, so it is done now; until then Stop
                // marks it done after the join, as it does the queued jobs.
                if (m_Joined)
                    m_LastDone = id;
                return id;
            }
            m_Queue.emplace_back(id, std::move(job));
        }
        m_Changed.notify_all();
        return id;
    }

    bool BackgroundWorker::IsDone(JobId id) const
    {
        std::lock_guard lock(m_Mutex);
        return id <= m_LastDone;
    }

    void BackgroundWorker::Wait(JobId id) const
    {
        std::unique_lock lock(m_Mutex);
        m_Changed.wait(lock, [&] { return id <= m_LastDone; });
    }

    void BackgroundWorker::Stop()
    {
        {
            std::lock_guard lock(m_Mutex);
            if (m_Stopped)
                return;
            m_Stopped = true;
        }
        m_Thread.request_stop();
        if (m_Thread.joinable())
            m_Thread.join();
        {
            std::lock_guard lock(m_Mutex);
            m_Queue.clear();
            m_LastDone = m_LastSubmitted;
            m_Joined   = true;
        }
        m_Changed.notify_all();
    }

    bool BackgroundWorker::IsStopped() const
    {
        std::lock_guard lock(m_Mutex);
        return m_Stopped;
    }

    void BackgroundWorker::Run(std::stop_token stop)
    {
        while (true)
        {
            JobId id = 0;
            Job   job;
            {
                std::unique_lock lock(m_Mutex);
                if (!m_Changed.wait(lock, stop, [this] { return !m_Queue.empty(); }))
                    return; // stop requested
                id  = m_Queue.front().first;
                job = std::move(m_Queue.front().second);
                m_Queue.pop_front();
            }
            // A job that throws is logged and counts as done, so the worker and its waiters carry on.
            try
            {
                if (job)
                    job(stop);
            }
            catch (const std::exception& e)
            {
                LOGERROR("[Worker] A background job failed: ", e.what());
            }
            {
                std::lock_guard lock(m_Mutex);
                m_LastDone = id;
            }
            m_Changed.notify_all();
            if (stop.stop_requested())
                return;
        }
    }
}

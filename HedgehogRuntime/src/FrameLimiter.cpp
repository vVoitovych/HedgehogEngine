#include "HedgehogRuntime/api/FrameLimiter.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <thread>

namespace Runtime
{
    namespace
    {
        constexpr std::chrono::nanoseconds SPIN_THRESHOLD = std::chrono::milliseconds(1);
    }

    std::chrono::nanoseconds ComputeFrameWait(std::chrono::steady_clock::time_point frameStart,
                                              std::chrono::steady_clock::time_point now, int maxFps)
    {
        using std::chrono::nanoseconds;
        if (maxFps <= 0)
        {
            return nanoseconds::zero();
        }
        const nanoseconds period  = nanoseconds(1'000'000'000 / maxFps);
        const nanoseconds elapsed = std::chrono::duration_cast<nanoseconds>(now - frameStart);
        return std::clamp(period - elapsed, nanoseconds::zero(), period);
    }

    FrameLimiter::FrameLimiter()
    {
        m_Timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!m_Timer)
        {
            // Before Windows 10 1803: a timer of the system tick (about 15 ms), so a short wait mostly spins.
            m_Timer = CreateWaitableTimerW(nullptr, TRUE, nullptr);
        }
    }

    FrameLimiter::~FrameLimiter()
    {
        if (m_Timer)
        {
            CloseHandle(m_Timer);
        }
    }

    void FrameLimiter::WaitFor(std::chrono::nanoseconds wait)
    {
        const auto deadline = std::chrono::steady_clock::now() + wait;
        if (wait > SPIN_THRESHOLD && m_Timer)
        {
            LARGE_INTEGER due;
            due.QuadPart = -((wait - SPIN_THRESHOLD).count() / 100); // relative, in 100 ns units
            if (SetWaitableTimer(m_Timer, &due, 0, nullptr, nullptr, FALSE))
            {
                WaitForSingleObject(m_Timer, INFINITE);
            }
        }
        while (std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::yield();
        }
    }
}

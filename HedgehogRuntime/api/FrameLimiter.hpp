#pragma once

#include <chrono>

namespace Runtime
{
    // How long a frame that began at frameStart still has to wait, at maxFps, to last one period:
    // zero without a cap or once the period has passed, never more than one period.
    [[nodiscard]] std::chrono::nanoseconds ComputeFrameWait(std::chrono::steady_clock::time_point frameStart,
                                                            std::chrono::steady_clock::time_point now, int maxFps);

    // Waits out a frame's remainder: a high-resolution timer for most of it, then a spin for the
    // last millisecond, which a timer's wake-up jitter would otherwise miss.
    class FrameLimiter
    {
    public:
        FrameLimiter();
        ~FrameLimiter();

        FrameLimiter(const FrameLimiter&)            = delete;
        FrameLimiter& operator=(const FrameLimiter&) = delete;
        FrameLimiter(FrameLimiter&&)                 = delete;
        FrameLimiter& operator=(FrameLimiter&&)      = delete;

        void WaitFor(std::chrono::nanoseconds wait);

    private:
        void* m_Timer = nullptr; // a Windows waitable timer, kept opaque so no Windows header leaks
    };
}

#include "HedgehogRuntime/api/FrameLimiter.hpp"

#include "doctest/doctest/doctest.h"

#include <chrono>

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

TEST_CASE("ComputeFrameWait waits out the rest of the period")
{
    const auto start  = Clock::now();
    const auto period = std::chrono::nanoseconds(16'666'666);

    CHECK(Runtime::ComputeFrameWait(start, start + 10ms, 0) == 0ns);
    CHECK(Runtime::ComputeFrameWait(start, start + 10ms, -5) == 0ns);
    CHECK(Runtime::ComputeFrameWait(start, start + 10ms, 60) == period - 10ms);
    CHECK(Runtime::ComputeFrameWait(start, start + period, 60) == 0ns);
    CHECK(Runtime::ComputeFrameWait(start, start + 30ms, 60) == 0ns);
    CHECK(Runtime::ComputeFrameWait(start, start - 5ms, 60) == period);
}

TEST_CASE("FrameLimiter waits at least as long as asked")
{
    Runtime::FrameLimiter limiter;

    auto begin = Clock::now();
    limiter.WaitFor(5ms);
    const auto waited = Clock::now() - begin;
    CHECK(waited >= 5ms);
    CHECK(waited < 1s); // generous: a loaded machine may preempt the waiting thread

    begin = Clock::now();
    limiter.WaitFor(0ns);
    CHECK(Clock::now() - begin < 1s);
}

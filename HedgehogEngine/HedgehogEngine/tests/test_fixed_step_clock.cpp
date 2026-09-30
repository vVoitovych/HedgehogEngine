#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Time/FixedStepClock.hpp"

#include <cmath>
#include <limits>

using HedgehogEngine::AdvanceFixedStepClock;
using HedgehogEngine::FixedStepClock;
using HedgehogEngine::ResetFixedStepClock;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;
}

TEST_CASE("FixedStepClock - defaults are 1/60 s steps and at most 8 per frame")
{
    const FixedStepClock clock;
    CHECK(clock.FixedDeltaTime == STEP);
    CHECK(clock.MaxSubsteps == 8u);
    CHECK(clock.TimeScale == 1.0f);
    CHECK(clock.Accumulator == 0.0);
    CHECK(clock.Time == 0.0);
}

TEST_CASE("FixedStepClock - 1/30 s gives exactly two 1/60 s steps")
{
    FixedStepClock clock;
    CHECK(AdvanceFixedStepClock(clock, 1.0f / 30.0f) == 2u);
    CHECK(clock.Accumulator < STEP * 1.0e-3);
    CHECK(clock.Time == doctest::Approx(2.0 * STEP));
    CHECK(clock.StepCount == 2u);
    CHECK(clock.FrameCount == 1u);
}

TEST_CASE("FixedStepClock - a short frame banks its time until a step is due")
{
    FixedStepClock clock;
    CHECK(AdvanceFixedStepClock(clock, STEP * 0.4f) == 0u);
    CHECK(AdvanceFixedStepClock(clock, STEP * 0.4f) == 0u);
    CHECK(AdvanceFixedStepClock(clock, STEP * 0.4f) == 1u);
    CHECK(clock.Accumulator == doctest::Approx(STEP * 0.2).epsilon(1e-4));
    CHECK(clock.FrameCount == 3u);
}

TEST_CASE("FixedStepClock - a long frame runs MaxSubsteps and drops the rest")
{
    FixedStepClock clock;
    CHECK(AdvanceFixedStepClock(clock, 1.0f) == 8u);
    CHECK(clock.Accumulator < STEP);
    CHECK(clock.Time == doctest::Approx(8.0 * STEP));

    clock.MaxSubsteps = 3;
    CHECK(AdvanceFixedStepClock(clock, 1.0f) == 3u);
}

TEST_CASE("FixedStepClock - TimeScale scales the steps and the simulated time")
{
    FixedStepClock half;
    half.TimeScale = 0.5f;
    uint32_t steps = 0;
    for (int frame = 0; frame < 60; ++frame)
        steps += AdvanceFixedStepClock(half, STEP);
    CHECK(steps == 30u);
    CHECK(half.Time == doctest::Approx(0.5).epsilon(1e-3));

    FixedStepClock stopped;
    stopped.TimeScale = 0.0f;
    CHECK(AdvanceFixedStepClock(stopped, 1.0f) == 0u);
    CHECK(stopped.Time == 0.0);
    CHECK(stopped.Accumulator == 0.0);

    FixedStepClock negative;
    negative.TimeScale = -2.0f;
    CHECK(AdvanceFixedStepClock(negative, 1.0f) == 0u);
    CHECK(negative.Time == 0.0);
}

TEST_CASE("FixedStepClock - a negative or non-finite frame time counts as zero")
{
    FixedStepClock clock;
    CHECK(AdvanceFixedStepClock(clock, -1.0f) == 0u);
    CHECK(AdvanceFixedStepClock(clock, std::numeric_limits<float>::quiet_NaN()) == 0u);
    CHECK(AdvanceFixedStepClock(clock, std::numeric_limits<float>::infinity()) == 0u);
    CHECK(clock.Accumulator == 0.0);
    CHECK(clock.Time == 0.0);
    CHECK(clock.FrameCount == 3u);
}

TEST_CASE("FixedStepClock - bad settings fall back to the defaults")
{
    FixedStepClock clock;
    clock.FixedDeltaTime = 0.0f;
    clock.MaxSubsteps    = 0;
    CHECK(AdvanceFixedStepClock(clock, 1.0f / 30.0f) == 2u);
    CHECK(clock.FixedDeltaTime == FixedStepClock::DEFAULT_FIXED_DELTA_TIME);
    CHECK(clock.MaxSubsteps == FixedStepClock::DEFAULT_MAX_SUBSTEPS);

    clock.FixedDeltaTime = -1.0f;
    (void)AdvanceFixedStepClock(clock, 0.0f);
    CHECK(clock.FixedDeltaTime == FixedStepClock::DEFAULT_FIXED_DELTA_TIME);
}

TEST_CASE("FixedStepClock - a custom step size is honoured")
{
    FixedStepClock clock;
    clock.FixedDeltaTime = 1.0f / 50.0f;
    CHECK(AdvanceFixedStepClock(clock, 1.0f / 25.0f) == 2u);
    CHECK(clock.Time == doctest::Approx(0.04));
}

TEST_CASE("FixedStepClock - Reset zeroes the state and keeps the settings")
{
    FixedStepClock clock;
    clock.FixedDeltaTime = 1.0f / 50.0f;
    clock.MaxSubsteps    = 4;
    clock.TimeScale      = 2.0f;
    (void)AdvanceFixedStepClock(clock, 0.1f);
    (void)AdvanceFixedStepClock(clock, 0.013f);
    REQUIRE(clock.StepCount > 0u);

    ResetFixedStepClock(clock);
    CHECK(clock.Accumulator == 0.0);
    CHECK(clock.Time == 0.0);
    CHECK(clock.FrameCount == 0u);
    CHECK(clock.StepCount == 0u);
    CHECK(clock.FixedDeltaTime == 1.0f / 50.0f);
    CHECK(clock.MaxSubsteps == 4u);
    CHECK(clock.TimeScale == 2.0f);
}

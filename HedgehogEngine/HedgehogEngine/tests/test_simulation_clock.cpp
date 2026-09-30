#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Simulation/SimulationClock.hpp"

#include <limits>

using HedgehogEngine::SimulationClock;
using HedgehogEngine::SimulationSteps;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;
}

TEST_CASE("SimulationClock - defaults to 1/60 s steps and 8 substeps")
{
    const SimulationClock clock;
    CHECK(clock.GetFixedDeltaTime() == doctest::Approx(STEP));
    CHECK(clock.GetMaxSubsteps() == 8u);
    CHECK(clock.GetTimeScale() == 1.0f);
    CHECK_FALSE(clock.IsPaused());
    CHECK(clock.GetTotalTime() == 0.0);
    CHECK(clock.GetFrameCount() == 0u);
    CHECK(clock.GetStepCount() == 0u);
}

TEST_CASE("SimulationClock - 1/30 s at a 1/60 s step runs two steps")
{
    SimulationClock clock;
    const SimulationSteps steps = clock.Advance(1.0f / 30.0f);
    CHECK(steps.Count == 2u);
    CHECK(steps.Alpha == doctest::Approx(0.0f).epsilon(0.001));
    CHECK(clock.GetTotalTime() == doctest::Approx(2.0 * STEP));
}

TEST_CASE("SimulationClock - exactly one step's time runs one step, every frame")
{
    SimulationClock clock;
    for (int i = 0; i < 600; ++i)
        REQUIRE(clock.Advance(STEP).Count == 1u);
    CHECK(clock.GetStepCount() == 600u);
    CHECK(clock.GetTotalTime() == doctest::Approx(10.0).epsilon(0.001));
}

TEST_CASE("SimulationClock - leftover time carries over and shows as alpha")
{
    SimulationClock clock;

    SimulationSteps steps = clock.Advance(STEP * 0.25f);
    CHECK(steps.Count == 0u);
    CHECK(steps.Alpha == doctest::Approx(0.25f));

    steps = clock.Advance(STEP * 0.5f);
    CHECK(steps.Count == 0u);
    CHECK(steps.Alpha == doctest::Approx(0.75f));

    steps = clock.Advance(STEP * 0.5f);
    CHECK(steps.Count == 1u);
    CHECK(steps.Alpha == doctest::Approx(0.25f));
}

TEST_CASE("SimulationClock - a long frame runs the max substeps and drops the rest")
{
    SimulationClock clock(STEP, 8);

    SimulationSteps steps = clock.Advance(1.0f);
    CHECK(steps.Count == 8u);
    CHECK(steps.Alpha == 0.0f);
    CHECK(clock.GetTotalTime() == doctest::Approx(8.0 * STEP));

    // Nothing of the dropped time is left for the next frame.
    steps = clock.Advance(0.0f);
    CHECK(steps.Count == 0u);
    CHECK(steps.Alpha == 0.0f);
}

TEST_CASE("SimulationClock - a time scale of 0.5 halves the steps")
{
    SimulationClock clock;
    clock.SetTimeScale(0.5f);

    uint32_t total = 0;
    for (int i = 0; i < 60; ++i)
        total += clock.Advance(STEP).Count;

    CHECK(total == 30u);
    CHECK(clock.GetTotalTime() == doctest::Approx(0.5).epsilon(0.001));
}

TEST_CASE("SimulationClock - a time scale of 2 doubles the steps")
{
    SimulationClock clock;
    clock.SetTimeScale(2.0f);
    CHECK(clock.Advance(STEP).Count == 2u);
}

TEST_CASE("SimulationClock - a paused clock runs nothing and does not advance")
{
    SimulationClock clock;
    (void)clock.Advance(STEP * 0.5f);
    const double totalBefore = clock.GetTotalTime();
    const auto   framesBefore = clock.GetFrameCount();

    clock.SetPaused(true);
    for (int i = 0; i < 10; ++i)
    {
        const SimulationSteps steps = clock.Advance(1.0f);
        CHECK(steps.Count == 0u);
    }
    CHECK(clock.GetTotalTime() == totalBefore);
    CHECK(clock.GetFrameCount() == framesBefore);
    CHECK(clock.GetStepCount() == 0u);

    // The half step banked before the pause is still there afterwards.
    clock.SetPaused(false);
    CHECK(clock.Advance(STEP * 0.5f).Count == 1u);
}

TEST_CASE("SimulationClock - zero, negative and non-finite frame times add nothing")
{
    SimulationClock clock;
    CHECK(clock.Advance(0.0f).Count == 0u);
    CHECK(clock.Advance(-1.0f).Count == 0u);
    CHECK(clock.Advance(std::numeric_limits<float>::quiet_NaN()).Count == 0u);
    CHECK(clock.Advance(std::numeric_limits<float>::infinity()).Count == 0u);
    CHECK(clock.GetTotalTime() == 0.0);
    CHECK(clock.GetFrameCount() == 4u);
}

TEST_CASE("SimulationClock - a negative time scale clamps to zero")
{
    SimulationClock clock;
    clock.SetTimeScale(-1.0f);
    CHECK(clock.GetTimeScale() == 0.0f);
    CHECK(clock.Advance(1.0f).Count == 0u);
}

TEST_CASE("SimulationClock - Reset clears time and counters but keeps settings")
{
    SimulationClock clock(1.0f / 30.0f, 4);
    clock.SetTimeScale(0.5f);
    (void)clock.Advance(0.5f);
    REQUIRE(clock.GetStepCount() > 0u);

    clock.Reset();
    CHECK(clock.GetTotalTime() == 0.0);
    CHECK(clock.GetFrameCount() == 0u);
    CHECK(clock.GetStepCount() == 0u);
    CHECK(clock.Advance(0.0f).Alpha == 0.0f);

    CHECK(clock.GetFixedDeltaTime() == doctest::Approx(1.0f / 30.0f));
    CHECK(clock.GetMaxSubsteps() == 4u);
    CHECK(clock.GetTimeScale() == 0.5f);
}

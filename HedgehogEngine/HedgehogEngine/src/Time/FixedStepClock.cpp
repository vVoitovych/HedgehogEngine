#include "HedgehogEngine/api/Time/FixedStepClock.hpp"

#include <algorithm>
#include <cmath>

namespace HedgehogEngine
{
    namespace
    {
        // Frame times and steps are floats, so 1/30 s can come out a hair short of two 1/60 s
        // steps. Anything within this fraction of a step counts as a step.
        constexpr double STEP_TOLERANCE = 1.0e-4;

        void SanitizeSettings(FixedStepClock& clock)
        {
            if (!(clock.FixedDeltaTime > 0.0f) || !std::isfinite(clock.FixedDeltaTime))
                clock.FixedDeltaTime = FixedStepClock::DEFAULT_FIXED_DELTA_TIME;
            if (clock.MaxSubsteps == 0)
                clock.MaxSubsteps = FixedStepClock::DEFAULT_MAX_SUBSTEPS;
        }

        double NonNegativeFinite(float value)
        {
            return std::isfinite(value) ? std::max(static_cast<double>(value), 0.0) : 0.0;
        }
    }

    uint32_t AdvanceFixedStepClock(FixedStepClock& clock, float realDeltaTime)
    {
        SanitizeSettings(clock);
        ++clock.FrameCount;

        clock.Accumulator += NonNegativeFinite(realDeltaTime) * NonNegativeFinite(clock.TimeScale);

        const double step      = static_cast<double>(clock.FixedDeltaTime);
        const double threshold = step * (1.0 - STEP_TOLERANCE);

        uint32_t steps = 0;
        while (clock.Accumulator >= threshold && steps < clock.MaxSubsteps)
        {
            clock.Accumulator = std::max(clock.Accumulator - step, 0.0);
            ++steps;
        }

        if (clock.Accumulator >= threshold)
            clock.Accumulator = 0.0; // over the substep budget: drop the rest

        clock.StepCount += steps;
        clock.Time += step * static_cast<double>(steps);
        return steps;
    }

    void ResetFixedStepClock(FixedStepClock& clock)
    {
        clock.Accumulator = 0.0;
        clock.Time        = 0.0;
        clock.FrameCount  = 0;
        clock.StepCount   = 0;
    }
}

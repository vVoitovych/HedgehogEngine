#include "HedgehogEngine/api/Simulation/SimulationClock.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace HedgehogEngine
{
    namespace
    {
        // Frame times and steps are floats, so 1/30 s can come out a hair short of
        // two 1/60 s steps. Anything within this fraction of a step counts as a step.
        constexpr double STEP_TOLERANCE = 1.0e-4;
    }

    SimulationClock::SimulationClock(float fixedDeltaTime, uint32_t maxSubsteps)
    {
        SetFixedDeltaTime(fixedDeltaTime);
        SetMaxSubsteps(maxSubsteps);
    }

    SimulationSteps SimulationClock::Advance(float realDeltaTime)
    {
        if (m_Paused)
            return {};

        ++m_FrameCount;

        const double frameTime = std::isfinite(realDeltaTime) ? std::max(realDeltaTime, 0.0f) : 0.0;
        m_Accumulator += frameTime * static_cast<double>(m_TimeScale);

        const double step      = static_cast<double>(m_FixedDeltaTime);
        const double threshold = step * (1.0 - STEP_TOLERANCE);

        SimulationSteps result;
        while (m_Accumulator >= threshold && result.Count < m_MaxSubsteps)
        {
            m_Accumulator = std::max(m_Accumulator - step, 0.0);
            ++result.Count;
        }

        if (m_Accumulator >= threshold)
            m_Accumulator = 0.0; // over the substep budget: drop the rest

        m_StepCount += result.Count;
        m_TotalTime += step * static_cast<double>(result.Count);
        result.Alpha = static_cast<float>(std::min(m_Accumulator / step, 1.0 - STEP_TOLERANCE));
        return result;
    }

    void SimulationClock::Reset()
    {
        m_Accumulator = 0.0;
        m_TotalTime   = 0.0;
        m_FrameCount  = 0;
        m_StepCount   = 0;
    }

    void SimulationClock::SetTimeScale(float timeScale)
    {
        m_TimeScale = std::isfinite(timeScale) ? std::max(timeScale, 0.0f) : 0.0f;
    }

    float SimulationClock::GetTimeScale() const
    {
        return m_TimeScale;
    }

    void SimulationClock::SetPaused(bool paused)
    {
        m_Paused = paused;
    }

    bool SimulationClock::IsPaused() const
    {
        return m_Paused;
    }

    void SimulationClock::SetFixedDeltaTime(float fixedDeltaTime)
    {
        assert(fixedDeltaTime > 0.0f && std::isfinite(fixedDeltaTime) && "Fixed delta time must be positive.");
        m_FixedDeltaTime = fixedDeltaTime;
    }

    float SimulationClock::GetFixedDeltaTime() const
    {
        return m_FixedDeltaTime;
    }

    void SimulationClock::SetMaxSubsteps(uint32_t maxSubsteps)
    {
        assert(maxSubsteps > 0 && "Max substeps must be at least one.");
        m_MaxSubsteps = maxSubsteps;
    }

    uint32_t SimulationClock::GetMaxSubsteps() const
    {
        return m_MaxSubsteps;
    }

    double SimulationClock::GetTotalTime() const
    {
        return m_TotalTime;
    }

    uint64_t SimulationClock::GetFrameCount() const
    {
        return m_FrameCount;
    }

    uint64_t SimulationClock::GetStepCount() const
    {
        return m_StepCount;
    }
}

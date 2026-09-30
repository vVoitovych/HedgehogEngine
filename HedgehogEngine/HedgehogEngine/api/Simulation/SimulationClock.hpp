#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include <cstdint>

namespace HedgehogEngine
{
    // What one Advance asks the simulation to do this frame.
    struct SimulationSteps
    {
        uint32_t Count = 0;    // fixed steps to run, at most the clock's max substeps
        float    Alpha = 0.0f; // leftover time as a fraction of a step, in [0, 1), for interpolation
    };

    // Turns variable frame times into a whole number of fixed steps. Real time is
    // scaled, added to an accumulator, and drained one fixed step at a time. A frame
    // that would need more than the max substeps runs that many and drops the rest,
    // so a slow frame can never make the next one slower still.
    class SimulationClock
    {
    public:
        static constexpr float    DEFAULT_FIXED_DELTA_TIME = 1.0f / 60.0f;
        static constexpr uint32_t DEFAULT_MAX_SUBSTEPS     = 8;

        SimulationClock() = default;
        HEDGEHOG_ENGINE_API SimulationClock(float fixedDeltaTime, uint32_t maxSubsteps);

        // A negative or non-finite realDeltaTime counts as zero. A paused clock
        // returns no steps and changes nothing.
        [[nodiscard]] HEDGEHOG_ENGINE_API SimulationSteps Advance(float realDeltaTime);

        // Back to time zero and frame zero; settings are kept.
        HEDGEHOG_ENGINE_API void Reset();

        HEDGEHOG_ENGINE_API void  SetTimeScale(float timeScale); // clamped to >= 0
        HEDGEHOG_ENGINE_API float GetTimeScale() const;

        HEDGEHOG_ENGINE_API void SetPaused(bool paused);
        HEDGEHOG_ENGINE_API bool IsPaused() const;

        HEDGEHOG_ENGINE_API void  SetFixedDeltaTime(float fixedDeltaTime); // must be > 0
        HEDGEHOG_ENGINE_API float GetFixedDeltaTime() const;

        HEDGEHOG_ENGINE_API void     SetMaxSubsteps(uint32_t maxSubsteps); // must be > 0
        HEDGEHOG_ENGINE_API uint32_t GetMaxSubsteps() const;

        // Simulated time: the sum of every fixed step run. Dropped time is not in it.
        HEDGEHOG_ENGINE_API double   GetTotalTime()  const;
        // Advance calls made while not paused.
        HEDGEHOG_ENGINE_API uint64_t GetFrameCount() const;
        // Fixed steps run since the last Reset.
        HEDGEHOG_ENGINE_API uint64_t GetStepCount()  const;

    private:
        float    m_FixedDeltaTime = DEFAULT_FIXED_DELTA_TIME;
        uint32_t m_MaxSubsteps    = DEFAULT_MAX_SUBSTEPS;
        float    m_TimeScale      = 1.0f;
        bool     m_Paused         = false;

        double   m_Accumulator = 0.0;
        double   m_TotalTime   = 0.0;
        uint64_t m_FrameCount  = 0;
        uint64_t m_StepCount   = 0;
    };
}

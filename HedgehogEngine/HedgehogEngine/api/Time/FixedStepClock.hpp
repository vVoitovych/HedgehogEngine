#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include <cstdint>

namespace HedgehogEngine
{
    // Turns each frame's real time into a whole number of fixed simulation steps. Plain data:
    // advance it with AdvanceFixedStepClock and reset it with ResetFixedStepClock. It has no pause
    // flag; whoever drives play mode simply stops advancing it.
    struct FixedStepClock
    {
        static constexpr float    DEFAULT_FIXED_DELTA_TIME = 1.0f / 60.0f;
        static constexpr uint32_t DEFAULT_MAX_SUBSTEPS     = 8;

        // Settings, kept by ResetFixedStepClock.
        float    FixedDeltaTime = DEFAULT_FIXED_DELTA_TIME;
        uint32_t MaxSubsteps    = DEFAULT_MAX_SUBSTEPS; // steps per frame at most; excess time is dropped
        float    TimeScale      = 1.0f;                 // 0 stops the steps; negative counts as 0

        // State.
        double   Accumulator = 0.0; // scaled time banked towards the next step, in seconds
        double   Time        = 0.0; // scaled simulated time: the fixed steps taken, in seconds
        uint64_t FrameCount  = 0;   // AdvanceFixedStepClock calls
        uint64_t StepCount   = 0;   // fixed steps taken
    };

    // Banks realDeltaTime * TimeScale and returns how many fixed steps to run this frame. It never
    // returns more than MaxSubsteps: time beyond that is dropped, so a long frame cannot snowball
    // (the "spiral of death"). A negative or non-finite frame time counts as zero. A frame within a
    // tiny tolerance of a whole number of steps counts as that many, so a float 1/30 s gives exactly
    // two 1/60 s steps. A non-positive FixedDeltaTime or MaxSubsteps is replaced by its default.
    [[nodiscard]] HEDGEHOG_ENGINE_API uint32_t AdvanceFixedStepClock(FixedStepClock& clock, float realDeltaTime);

    // Zeroes the accumulator, the time and the counters, and keeps the settings.
    HEDGEHOG_ENGINE_API void ResetFixedStepClock(FixedStepClock& clock);
}

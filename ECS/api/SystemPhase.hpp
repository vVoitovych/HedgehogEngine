#pragma once

#include <cstddef>
#include <cstdint>

namespace ECS
{
    // Where in the frame a system's OnFrame runs. ECS::RunPhases visits the phases in this order,
    // and the systems of one phase in registration order.
    enum class SystemPhase : uint8_t
    {
        Input,      // Input that gameplay reads this frame (the game UI's hit testing).
        Simulation, // Gameplay: the play-mode fixed steps and update run first, then these systems.
        Animation,  // Poses from what gameplay chose this frame.
        Transform,  // Local, then world matrices.
        Late,       // Reads final transforms: lights, audio.
        Sync,       // Hands the frame's results to what renders them.
    };

    inline constexpr size_t SYSTEM_PHASE_COUNT = static_cast<size_t>(SystemPhase::Sync) + 1;
}

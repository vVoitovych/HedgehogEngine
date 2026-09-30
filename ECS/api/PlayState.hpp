#pragma once

#include <cstdint>

namespace ECS
{
    // Whether gameplay runs. The ECS runs its systems' play-mode hooks only while Playing.
    enum class PlayState : uint8_t
    {
        Edit,    // the scene is being edited; no gameplay hook runs
        Playing,
        Paused   // playing, but frozen: no update hook runs
    };
}

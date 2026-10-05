#pragma once

#include <cstdint>

namespace ECS
{
    // The play state as the ECS sees it; whoever drives play mode maps its own state onto it.
    enum class PlayMode : uint8_t
    {
        Edit,
        Playing,
        Paused,
    };

    // What one frame hands every system's OnFrame. Plain data, filled by whoever runs the frame
    // (the engine): the ECS computes nothing here. Not HedgehogEngine::FrameContext, which holds a
    // view's camera matrices; always write this one as ECS::FrameContext.
    struct FrameContext
    {
        // The real time since the last frame, in seconds.
        float DeltaTime = 0.0f;
        // DeltaTime times the game's time scale: the time gameplay advances by this frame.
        float ScaledDeltaTime = 0.0f;
        // The length of one fixed simulation step, in seconds.
        float FixedDeltaTime = 0.0f;
        // How many fixed steps run this frame (zero or more), as the caller's clock decided.
        uint32_t FixedSteps = 0;
        PlayMode Mode = PlayMode::Edit;
    };
}

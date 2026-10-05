#pragma once

#include "HedgehogInput/api/ActionState.hpp"
#include "HedgehogInput/api/InputActionMap.hpp"
#include "HedgehogMath/api/Vector.hpp"

namespace HedgehogEngine
{
    // The game's input for the Input phase, an ECS service the EngineContext owns and fills in
    // UpdateGameInput: the Game action map, the action state it was just evaluated into (systems may
    // consume actions from it, so later systems and gameplay no longer see them) and the size of the
    // game view in pixels, the size the UI is extracted at (zero gives the UI no input).
    struct GameInputFrame
    {
        const HInput::InputActionMap* Map      = nullptr;
        HInput::ActionState*          Actions  = nullptr;
        HM::Vector2                   ViewSize = HM::Vector2(0.0f, 0.0f);
    };
}

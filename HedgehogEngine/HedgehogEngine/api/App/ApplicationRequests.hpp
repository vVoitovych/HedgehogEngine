#pragma once

#include "HedgehogEngine/HedgehogWindow/api/CursorMode.hpp"

namespace HedgehogEngine
{
    // What the running game asks of the application, an ECS service the EngineContext owns.
    // Scripts set QuitRequested (Application.quit()); the host acts on it after the frame: the
    // game loop ends, the editor stops Play. Scripts also set Cursor (the Cursor table); the game
    // runtime applies it to its window and locks the game's pointer while it is Locked; the editor
    // does not apply it yet. Play and Stop clear both. IsEditor is set once by the Editor and never
    // cleared.
    struct ApplicationRequests
    {
        bool           QuitRequested = false;
        HW::CursorMode Cursor        = HW::CursorMode::Normal;
        bool           IsEditor      = false;
    };
}

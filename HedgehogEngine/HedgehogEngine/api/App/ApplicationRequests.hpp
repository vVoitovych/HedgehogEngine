#pragma once

namespace HedgehogEngine
{
    // What the running game asks of the application, an ECS service the EngineContext owns.
    // Scripts set QuitRequested (Application.quit()); the host acts on it after the frame: the
    // game loop ends, the editor stops Play. Play and Stop clear it. IsEditor is set once by the
    // Editor and never cleared.
    struct ApplicationRequests
    {
        bool QuitRequested = false;
        bool IsEditor      = false;
    };
}

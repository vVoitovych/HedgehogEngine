#pragma once

#include <string>

namespace HedgehogEngine
{
    /// Emitted once EngineContext::RequestSceneLoad's scene has replaced the world at the end of
    /// the frame. Path is the scene's virtual path.
    struct SceneLoadedEvent
    {
        std::string Path;
    };
}

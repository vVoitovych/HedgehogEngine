#pragma once

#include <cstdint>
#include <string>

namespace Editor
{
    // Editor.exe --game-mode [frames] [scene.yaml]: runs the engine the way a game build will, with
    // no editor, through HedgehogRuntime's GameRuntime (the loop a Game executable will run) at a
    // fixed 1/60 s step. It loads sceneFile from Assets/Scenes, or when it is empty the startup
    // scene of the project settings (project://Project.yaml), else Default.yaml, plays it,
    // and renders every frame with RenderFrame from views derived from the scene's camera
    // components. The settings file is read but never written.
    //
    // No ImGui context is ever created. Returns nonzero on any Vulkan validation error, or if an
    // ImGui context exists at any point: the renderer not depending on ImGui is checked, not assumed.
    int RunGameMode(uint32_t frames, const std::string& sceneFile);
}

#pragma once

#include <cstdint>

namespace Editor
{
    // Editor.exe --game-mode [frames]: runs the engine the way a game build will, with no editor.
    // It loads the default scene, builds a Renderer with the render-graph path only, and renders
    // every frame with RenderFrame from views derived from the scene's camera components, forcing
    // RenderingSettings::GetUseRenderGraph on for the run (the settings file is never written).
    //
    // No ImGui context is ever created. Returns nonzero on any Vulkan validation error, or if an
    // ImGui context exists at any point: the renderer not depending on ImGui is checked, not assumed.
    int RunGameMode(uint32_t frames);
}

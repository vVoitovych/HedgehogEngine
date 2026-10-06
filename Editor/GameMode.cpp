#include "GameMode.hpp"

#include "HedgehogRuntime/api/GameRuntime.hpp"
#include "HedgehogRenderer/Renderer.hpp"

#include "Logger/api/Logger.hpp"

#include "imgui.h"

#include <cstdlib>

namespace Editor
{
    namespace
    {
        constexpr const char* SCENE_DIRECTORY = "assets://Scenes/";
        constexpr const char* FALLBACK_SCENE  = "Default.yaml";

        // A fixed step keeps runs comparable: nothing here measures time.
        constexpr float FRAME_TIME = 1.0f / 60.0f;

        struct RunResult
        {
            bool Started  = false;
            bool SawImGui = false; // an ImGui context existed at some point
        };

        // Runs the game runtime. It is gone when this returns, so validation errors from its
        // teardown are counted too.
        RunResult RenderFrames(uint32_t frames, const std::string& sceneFile)
        {
            Runtime::RuntimeDesc desc;
            desc.ScenePath         = sceneFile.empty() ? std::string() : SCENE_DIRECTORY + sceneFile;
            desc.FallbackScenePath = std::string(SCENE_DIRECTORY) + FALLBACK_SCENE;
            desc.MaxFrames         = frames;
            desc.FixedFrameTime    = FRAME_TIME;

            RunResult result;
            result.SawImGui = ImGui::GetCurrentContext() != nullptr;
            {
                Runtime::GameRuntime runtime;
                result.Started = runtime.Init(desc);
                while (runtime.RunFrame())
                    result.SawImGui = result.SawImGui || ImGui::GetCurrentContext() != nullptr;
                runtime.Shutdown();
            }
            result.SawImGui = result.SawImGui || ImGui::GetCurrentContext() != nullptr;
            return result;
        }
    }

    int RunGameMode(uint32_t frames, const std::string& sceneFile)
    {
        LOGINFO("Game mode: rendering ", frames, " frame(s) of ", sceneFile.empty() ? "the startup scene" : sceneFile,
                " through the render graph...");
        if (!Renderer::AreValidationLayersEnabled())
            LOGWARNING("Game mode: Vulkan validation layers are disabled in this build; only a crash-free run "
                       "is being verified. Use a Debug build for full coverage.");

        const RunResult result = RenderFrames(frames, sceneFile);

        // Counted after teardown, so errors such as leaked Vulkan objects are included.
        const uint32_t errors   = Renderer::GetValidationErrorCount();
        const uint32_t warnings = Renderer::GetValidationWarningCount();
        if (!result.Started)
        {
            LOGERROR("Game mode FAILED: the game runtime did not start. See the log above for details.");
            return EXIT_FAILURE;
        }
        if (result.SawImGui)
        {
            LOGERROR("Game mode FAILED: an ImGui context was created. The render-graph path must not depend on "
                     "ImGui.");
            return EXIT_FAILURE;
        }
        if (errors > 0)
        {
            LOGERROR("Game mode FAILED: ", errors, " Vulkan validation error(s), ", warnings,
                     " warning(s). See the log above for details.");
            return EXIT_FAILURE;
        }

        LOGINFO("Game mode PASSED: 0 validation errors, ", warnings, " warning(s), no ImGui context.");
        return EXIT_SUCCESS;
    }
}

#include "GameMode.hpp"

#include "HedgehogEngine/api/Engine.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/WindowContext.hpp"
#include "HedgehogEngine/HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/RenderingSettings.hpp"
#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"
#include "HedgehogExtract/api/MeshBounds.hpp"
#include "HedgehogExtract/api/RenderScene.hpp"
#include "HedgehogExtract/api/SceneExtractor.hpp"
#include "HedgehogRenderer/Renderer.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "Logger/api/Logger.hpp"

#include "imgui.h"

#include <cstdlib>

namespace Editor
{
    namespace
    {
        constexpr const char* ENGINE_SETTINGS_PATH = "engine://engine_settings.yaml";
        constexpr const char* GAME_SCENE           = "engine://Assets/Scenes/Default.yaml";

        // A fixed step keeps runs comparable: nothing here measures time.
        constexpr float FRAME_TIME = 1.0f / 60.0f;

        // Renders frames and reports whether an ImGui context ever existed.
        bool RenderFrames(uint32_t frames)
        {
            HedgehogEngine::Engine engine;
            auto&       engineContext = engine.GetEngineContext();
            auto&       settings      = engineContext.GetSettings();
            const auto& fileSystem    = engineContext.GetFileSystem();

            if (fileSystem.Exists(ENGINE_SETTINGS_PATH) && !settings.Load(ENGINE_SETTINGS_PATH, fileSystem))
                LOGWARNING("Game mode: engine settings could not be read, using defaults.");
            settings.CleanDirtyState();
            settings.GetRenderingSettings().SetUseRenderGraph(true); // for this run only; never saved

            const auto scenePath = fileSystem.ResolvePhysical(GAME_SCENE);
            if (!scenePath || !engineContext.GetSceneManager().LoadScene(scenePath->string()))
                LOGWARNING("Game mode: could not load '", GAME_SCENE, "'; rendering an empty scene.");

            Renderer::Renderer renderer(engine.GetWindowContext().GetWindow(), settings, fileSystem,
                                        Renderer::RendererPaths::RenderGraphOnly);

            HX::RenderScene          scene;
            HX::MeshBoundsCache      meshBounds;
            const HX::SceneExtractor extractor;
            bool                     sawImGui = ImGui::GetCurrentContext() != nullptr;

            auto& windowContext = engine.GetWindowContext();
            for (uint32_t frame = 0; frame < frames && !windowContext.ShouldClose(); ++frame)
            {
                windowContext.HandleInput();
                engine.UpdateContext(FRAME_TIME, renderer.GetAspectRatio());

                meshBounds.Update(engineContext.GetResourceCatalog());
                scene.Clear();
                extractor.Extract(engineContext.GetECS(), *engineContext.GetRenderSystem(),
                                  *engineContext.GetLightSystem(), *engineContext.GetCameraSystem(), scene,
                                  meshBounds.GetBounds());

                renderer.SyncResources(engineContext.GetResourceCatalog());
                renderer.RenderFrame(scene, settings);
                sawImGui = sawImGui || ImGui::GetCurrentContext() != nullptr;
            }

            renderer.Cleanup();
            engine.Cleanup();
            return sawImGui || ImGui::GetCurrentContext() != nullptr;
        }
    }

    int RunGameMode(uint32_t frames)
    {
        LOGINFO("Game mode: rendering ", frames, " frame(s) through the render graph...");
        if (!Renderer::AreValidationLayersEnabled())
            LOGWARNING("Game mode: Vulkan validation layers are disabled in this build; only a crash-free run "
                       "is being verified. Use a Debug build for full coverage.");

        const bool sawImGui = RenderFrames(frames);

        // Counted after teardown, so errors such as leaked Vulkan objects are included.
        const uint32_t errors   = Renderer::GetValidationErrorCount();
        const uint32_t warnings = Renderer::GetValidationWarningCount();
        if (sawImGui)
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

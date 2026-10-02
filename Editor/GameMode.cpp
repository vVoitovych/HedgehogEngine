#include "GameMode.hpp"

#include "HedgehogEngine/api/Engine.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/WindowContext.hpp"
#include "HedgehogEngine/HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"
#include "HedgehogExtract/api/MeshBounds.hpp"
#include "HedgehogExtract/api/RenderScene.hpp"
#include "HedgehogExtract/api/SceneExtractor.hpp"
#include "HedgehogEngine/api/Containers/FontContainer.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"
#include "HedgehogRenderer/Renderer.hpp"
#include "HedgehogScripting/api/ScriptSystem.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "Logger/api/Logger.hpp"

#include "imgui.h"

#include <algorithm>
#include <cstdlib>

namespace Editor
{
    namespace
    {
        constexpr const char* ENGINE_SETTINGS_PATH = "engine://engine_settings.yaml";
        constexpr const char* SCENE_DIRECTORY      = "engine://Assets/Scenes/";

        // A fixed step keeps runs comparable: nothing here measures time.
        constexpr float FRAME_TIME = 1.0f / 60.0f;

        // Renders frames and reports whether an ImGui context ever existed.
        bool RenderFrames(uint32_t frames, const std::string& sceneFile)
        {
            HedgehogEngine::Engine engine;
            auto&       engineContext = engine.GetEngineContext();
            auto&       settings      = engineContext.GetSettings();
            const auto& fileSystem    = engineContext.GetFileSystem();

            // Before the scene loads, as the Editor does; the engine's ECS owns it.
            (void)HedgehogScripting::RegisterScriptSystem(engineContext, fileSystem);

            if (fileSystem.Exists(ENGINE_SETTINGS_PATH) && !settings.Load(ENGINE_SETTINGS_PATH, fileSystem))
                LOGWARNING("Game mode: engine settings could not be read, using defaults.");
            settings.CleanDirtyState();

            const std::string scene     = SCENE_DIRECTORY + sceneFile;
            const auto        scenePath = fileSystem.ResolvePhysical(scene);
            if (!scenePath || !engineContext.GetSceneManager().LoadScene(scenePath->string()))
                LOGWARNING("Game mode: could not load '", scene, "'; rendering an empty scene.");

            // A game runs its gameplay: the scene's scripts play from the first frame.
            (void)engineContext.Play();

            Renderer::Renderer renderer(engine.GetWindowContext().GetWindow(), fileSystem);

            HX::RenderScene          renderScene;
            HX::MeshBoundsCache      meshBounds;
            const HX::SceneExtractor extractor;
            bool                     sawImGui = ImGui::GetCurrentContext() != nullptr;

            auto& windowContext = engine.GetWindowContext();
            for (uint32_t frame = 0; frame < frames && !windowContext.ShouldClose(); ++frame)
            {
                windowContext.HandleInput();
                int width = 0, height = 0;
                windowContext.GetWindow().GetFramebufferSize(width, height);
                engine.UpdateContext(FRAME_TIME, static_cast<float>(std::max(width, 1)) / static_cast<float>(std::max(height, 1)));

                meshBounds.Update(engineContext.GetResourceCatalog());
                renderScene.Clear();
                extractor.Extract(engineContext.GetECS(), *engineContext.GetRenderSystem(),
                                  *engineContext.GetLightSystem(), *engineContext.GetCameraSystem(), renderScene,
                                  meshBounds.GetBounds());
                extractor.ExtractUi(engineContext.GetECS(), *engineContext.GetUiSystem(),
                                    HM::Vector2(static_cast<float>(width), static_cast<float>(height)), renderScene,
                                    &engineContext.GetResourceCatalog().GetFontContainer());

                renderer.SyncResources(engineContext.GetResourceCatalog());
                renderer.RenderFrame(renderScene, settings);
                sawImGui = sawImGui || ImGui::GetCurrentContext() != nullptr;
            }

            (void)engineContext.Stop();
            renderer.Cleanup();
            engine.Cleanup();
            return sawImGui || ImGui::GetCurrentContext() != nullptr;
        }
    }

    int RunGameMode(uint32_t frames, const std::string& sceneFile)
    {
        LOGINFO("Game mode: rendering ", frames, " frame(s) of ", sceneFile, " through the render graph...");
        if (!Renderer::AreValidationLayersEnabled())
            LOGWARNING("Game mode: Vulkan validation layers are disabled in this build; only a crash-free run "
                       "is being verified. Use a Debug build for full coverage.");

        const bool sawImGui = RenderFrames(frames, sceneFile);

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

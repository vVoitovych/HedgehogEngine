#include "EditorGui.hpp"

#include "HedgehogEngine/api/ECS/components/NavMeshComponent.hpp"
#include "HedgehogEngine/api/Engine.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Navigation/NavMeshFile.hpp"
#include "HedgehogEngine/api/Navigation/NavigationGeometry.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "HedgehogNavigation/api/NavMeshBake.hpp"

#include "DialogueWindows/api/NavMeshDialogue.hpp"

#include "EcsSerialization/api/ComponentTypeRegistry.hpp"
#include "ECS/api/ECS.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "Logger/api/Logger.hpp"

#include "imgui.h"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Editor
{
    namespace
    {
        constexpr std::string_view ASSETS_PREFIX     = "assets://";
        constexpr const char*      NAVIGATION_FOLDER = "Navigation";
        constexpr std::string_view NAVMESH_EXTENSION = ".navmesh";
    }

    void EditorGui::DrawNavMeshComponent(HedgehogEngine::Engine& context)
    {
        auto&             engineContext = context.GetEngineContext();
        auto&             ecs           = engineContext.GetECS();
        const ECS::Entity entity        = m_SelectedEntity.value();
        const auto*       info          = engineContext.GetComponentTypes().Find("NavMeshComponent");
        if (!info || !DrawRegisteredComponent(context, *info) || !ecs.HasComponent<HedgehogEngine::NavMeshComponent>(entity))
            return;
        ImGui::BeginDisabled(engineContext.GetPlayState() != HedgehogEngine::PlayState::Edit);
        if (ImGui::Button("Bake"))
            BakeNavMesh(engineContext, entity);
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Bake the scene's static colliders and terrains into a .navmesh file (Edit mode only)");
    }

    void EditorGui::BakeNavMesh(HedgehogEngine::EngineContext& engineContext, ECS::Entity entity)
    {
        auto&      fileSystem = engineContext.GetFileSystem();
        const auto folder     = fileSystem.ResolvePhysical(std::string(ASSETS_PREFIX) + NAVIGATION_FOLDER);
        if (!folder)
            return;
        std::error_code error;
        std::filesystem::create_directories(*folder, error);
        const std::string fileName  = engineContext.GetSceneManager().GetSceneName() + std::string(NAVMESH_EXTENSION);
        const std::string suggested = (std::filesystem::path(*folder) / fileName).make_preferred().string();
        const char*       picked    = DialogueWindows::NavMeshSaveDialogue(suggested.c_str());
        if (!picked)
            return;
        std::filesystem::path chosen(picked);
        if (chosen.extension() != NAVMESH_EXTENSION)
            chosen += NAVMESH_EXTENSION;
        const auto virtualPath = fileSystem.ToVirtualPath(chosen.string());
        if (!virtualPath || !virtualPath->starts_with(ASSETS_PREFIX))
        {
            LOGERROR("[Navigation] '" + chosen.string() + "' is not under assets://, so it cannot be a navmesh.");
            return;
        }

        // On the main thread, as Build Game is: the editor waits.
        auto& component = engineContext.GetECS().GetComponent<HedgehogEngine::NavMeshComponent>(entity);
        const HN::NavMeshBuildSettings settings = HedgehogEngine::MakeNavMeshBuildSettings(component);
        const auto                     start    = std::chrono::steady_clock::now();
        const HN::NavMeshBakeResult    baked    = HN::BakeNavMesh(
            HedgehogEngine::GatherNavigationGeometry(engineContext.GetECS(), engineContext.GetResourceCatalog(),
                                                     { component.IncludeRenderMeshes }),
            settings);
        const auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        if (!baked.Error.empty())
        {
            LOGERROR("[Navigation] " + baked.Error);
            return;
        }
        const std::vector<uint8_t> bytes = HedgehogEngine::WriteNavMeshFile(settings, baked.Data);
        std::vector<std::byte>     data(bytes.size());
        for (size_t i = 0; i < bytes.size(); ++i)
            data[i] = static_cast<std::byte>(bytes[i]);
        if (!fileSystem.WriteFile(*virtualPath, data))
        {
            LOGERROR("[Navigation] " + *virtualPath + " cannot be written.");
            return;
        }
        component.NavMesh = virtualPath->substr(ASSETS_PREFIX.size());
        LOGINFO("[Navigation] Baked " + *virtualPath + ": " + std::to_string(baked.PolygonCount) + " polygons in " +
                std::to_string(milliseconds) + " ms.");
    }
}

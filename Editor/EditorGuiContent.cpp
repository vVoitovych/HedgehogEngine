#include "EditorGui.hpp"
#include "Panels/AssetDragDrop.hpp"
#include "Panels/ContentPanel.hpp"
#include "Platform/ShellActions.hpp"
#include "Tools/PipelineWindow.hpp"
#include "Tools/RenderGraphEditor/GraphFileReference.hpp"
#include "Tools/RenderGraphEditor/RenderGraphEditorWindow.hpp"
#include "Tools/ShaderWindow.hpp"
#include "Tools/VertexDescriptionWindow.hpp"

#include "HedgehogEngine/api/Containers/MaterialContainer.hpp"
#include "HedgehogEngine/api/ECS/components/CameraComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/ScriptSystem.hpp"
#include "HedgehogEngine/api/Engine.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogRenderer/Renderer.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"
#include "Logger/api/Logger.hpp"

#include "imgui.h"

#include <string_view>
#include <utility>

namespace Editor
{
    namespace
    {
        constexpr std::string_view ASSETS_PREFIX    = "assets://";
        constexpr const char*      OPEN_SCENE_POPUP = "Open dropped scene";

        // What the selected entity needs for a file of this type to go onto it.
        const char* RequiredComponent(ContentType type)
        {
            switch (type)
            {
            case ContentType::Mesh:     return "Mesh";
            case ContentType::Material:
            case ContentType::Texture:  return "Render (with a material)";
            case ContentType::Script:   return "Script";
            default:                    return nullptr;
            }
        }
    }

    void EditorGui::OpenContentItem(HedgehogEngine::Engine& context, const ContentOpenRequest& request)
    {
        auto&       engineContext = context.GetEngineContext();
        const auto& fileSystem    = engineContext.GetFileSystem();
        const std::optional<std::filesystem::path> resolved = fileSystem.ResolvePhysical(request.VirtualPath);
        if (!resolved)
            return;
        const std::filesystem::path physical     = std::filesystem::path(*resolved).make_preferred();
        const std::string           physicalPath = physical.string();

        switch (request.Type)
        {
        case ContentType::Scene:
            // As File > Open does.
            if (engineContext.GetSceneManager().LoadScene(physicalPath))
            {
                RecordLastScene(physicalPath, fileSystem);
                m_SelectedEntity.reset();
            }
            return;
        case ContentType::Shader:
            m_ShaderWindow->OpenPath(physicalPath, fileSystem);
            return;
        case ContentType::Pipeline:
            m_PipelineWindow->OpenPath(physicalPath, fileSystem);
            return;
        case ContentType::VertexDescription:
            m_VertexDescWindow->OpenPath(physicalPath, fileSystem);
            return;
        case ContentType::RenderGraph:
            if (m_Renderer)
            {
                const std::string reference = MakeGraphReference(physical, *m_Renderer, fileSystem);
                (void)m_Renderer->LoadGraph(reference); // one that fails to load shows why in the editor
                m_RenderGraphEditorWindow->OpenGraph(reference);
            }
            return;
        case ContentType::Mesh:
        case ContentType::Material:
        case ContentType::Texture:
        case ContentType::Script:
            if (AssignToSelection(context, request, physicalPath))
                return;
            LOGINFO("Content: select an entity with a ", RequiredComponent(request.Type), " component to assign '",
                    request.VirtualPath, "' to it; opening it in its default application instead.");
            break;
        case ContentType::Folder:
        case ContentType::Other:
            break;
        }

        if (!OpenWithDefaultApplication(physical))
            LOGWARNING("Content: no application opened '", physicalPath, "'.");
    }

    bool EditorGui::AssignToSelection(HedgehogEngine::Engine& context, const ContentOpenRequest& request,
                                      const std::string& physicalPath)
    {
        if (!m_SelectedEntity || !request.VirtualPath.starts_with(ASSETS_PREFIX))
            return false;
        auto&             engineContext = context.GetEngineContext();
        auto&             ecs           = engineContext.GetECS();
        const ECS::Entity entity        = *m_SelectedEntity;
        // The mesh, material and texture containers key their files by path under assets://.
        const std::string relativePath = request.VirtualPath.substr(ASSETS_PREFIX.size());

        switch (request.Type)
        {
        case ContentType::Mesh:
            if (!ecs.HasComponent<HedgehogEngine::MeshComponent>(entity))
                return false;
            engineContext.GetMeshSystem()->LoadMesh(ecs, entity, relativePath);
            break;
        case ContentType::Material:
        {
            if (!ecs.HasComponent<HedgehogEngine::RenderComponent>(entity))
                return false;
            auto& render    = ecs.GetComponent<HedgehogEngine::RenderComponent>(entity);
            render.Material = relativePath;
            engineContext.GetRenderSystem()->Update(ecs, entity);
            break;
        }
        case ContentType::Texture:
        {
            if (!ecs.HasComponent<HedgehogEngine::RenderComponent>(entity))
                return false;
            const auto& render = ecs.GetComponent<HedgehogEngine::RenderComponent>(entity);
            if (!render.MaterialIndex)
                return false;
            engineContext.GetResourceCatalog().GetMaterialContainer().LoadBaseTexture(*render.MaterialIndex, relativePath);
            break;
        }
        case ContentType::Script:
            if (!ecs.HasComponent<HedgehogEngine::ScriptComponent>(entity))
                return false;
            engineContext.GetScriptSystem()->ChangeScript(entity, ecs, engineContext.GetEventBus(), engineContext.GetFileSystem(),
                                                          physicalPath);
            break;
        case ContentType::RenderGraph:
        {
            if (!ecs.HasComponent<HedgehogEngine::CameraComponent>(entity) || !m_Renderer)
                return false;
            auto& camera     = ecs.GetComponent<HedgehogEngine::CameraComponent>(entity);
            camera.GraphName = MakeGraphReference(physicalPath, *m_Renderer, engineContext.GetFileSystem());
            (void)m_Renderer->LoadGraph(camera.GraphName);
            break;
        }
        default:
            return false;
        }
        // A mesh or material not loaded before gets a new index that the containers only learn of
        // in the next frame's catalog update; the inspector reads it this frame, so sync now.
        engineContext.GetResourceCatalog().Update(*engineContext.GetRenderSystem(), *engineContext.GetMeshSystem());
        LOGINFO("Content: assigned '", request.VirtualPath, "' to the selected entity.");
        return true;
    }

    void EditorGui::AcceptSelectionDrop(ContentType type)
    {
        if (const auto drop = AcceptAssetDrop({ type }))
            m_AssetDrop = AssetDrop{ *drop, false, std::nullopt };
    }

    void EditorGui::ApplyAssetDrop(HedgehogEngine::Engine& context)
    {
        if (!m_AssetDrop)
            return;
        const AssetDrop drop = *std::exchange(m_AssetDrop, std::nullopt);

        if (drop.OnHierarchy)
        {
            if (drop.Asset.Type == ContentType::Scene)
                m_SceneToOpen = drop.Asset; // DrawOpenSceneDropPopup asks first
            else
                CreateMeshEntity(context, drop.Asset, drop.Parent);
            return;
        }

        const std::optional<std::filesystem::path> physical =
            context.GetEngineContext().GetFileSystem().ResolvePhysical(drop.Asset.VirtualPath);
        if (!physical || !AssignToSelection(context, drop.Asset, std::filesystem::path(*physical).make_preferred().string()))
            LOGWARNING("Content: '", drop.Asset.VirtualPath, "' could not be assigned to the selected entity.");
    }

    void EditorGui::CreateMeshEntity(HedgehogEngine::Engine& context, const ContentOpenRequest& mesh,
                                     std::optional<ECS::Entity> parent)
    {
        if (!mesh.VirtualPath.starts_with(ASSETS_PREFIX))
            return;
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        auto* meshSystem    = engineContext.GetMeshSystem();
        auto* renderSystem  = engineContext.GetRenderSystem();

        const ECS::Entity entity = engineContext.GetSceneManager().CreateGameObject(parent);
        ecs.GetComponent<ECS::HierarchyComponent>(entity).Name = std::filesystem::path(mesh.VirtualPath).stem().string();
        ecs.AddComponent(entity, HedgehogEngine::MeshComponent{ mesh.VirtualPath.substr(ASSETS_PREFIX.size()) });
        meshSystem->Update(ecs, entity, engineContext.GetFileSystem());
        // The engine has no default material, and an entity without one is never drawn
        // (SceneExtractor skips it): use the first material the scene already uses.
        HedgehogEngine::RenderComponent render;
        if (!renderSystem->GetMaterials().empty())
            render.Material = renderSystem->GetMaterials().front();
        else
            LOGWARNING("Content: no material is loaded yet; give '", mesh.VirtualPath, "' one so it is drawn.");
        ecs.AddComponent(entity, render);
        renderSystem->Update(ecs, entity);
        // As when assigning: the containers must know the new mesh before anything reads its index.
        engineContext.GetResourceCatalog().Update(*renderSystem, *meshSystem);

        m_SelectedEntity = entity;
        LOGINFO("Content: created '", ecs.GetComponent<ECS::HierarchyComponent>(entity).Name, "' from '", mesh.VirtualPath, "'.");
    }

    void EditorGui::DrawOpenSceneDropPopup(HedgehogEngine::Engine& context)
    {
        if (m_SceneToOpen && !ImGui::IsPopupOpen(OPEN_SCENE_POPUP))
            ImGui::OpenPopup(OPEN_SCENE_POPUP);
        if (!ImGui::BeginPopupModal(OPEN_SCENE_POPUP, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            return;

        const std::string name = std::filesystem::path(m_SceneToOpen ? m_SceneToOpen->VirtualPath : std::string{}).filename().string();
        ImGui::Text("Open the scene '%s'? It replaces the current scene.", name.c_str());
        bool close = false;
        if (ImGui::Button("Open"))
        {
            if (m_SceneToOpen)
                OpenContentItem(context, *m_SceneToOpen);
            close = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            close = true;
        if (close)
        {
            m_SceneToOpen.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

#include "EditorGui.hpp"
#include "Panels/AssetDragDrop.hpp"
#include "Panels/ContentPanel.hpp"
#include "Platform/ShellActions.hpp"
#include "Tools/PipelineWindow.hpp"
#include "Tools/RenderGraphEditor/GraphFileReference.hpp"
#include "Tools/RenderGraphEditor/RenderGraphEditorWindow.hpp"
#include "Tools/ShaderWindow.hpp"
#include "Tools/InputActionsWindow.hpp"
#include "Tools/VertexDescriptionWindow.hpp"

#include "HedgehogEngine/api/Containers/MaterialContainer.hpp"
#include "HedgehogEngine/api/ECS/components/AudioSourceComponent.hpp"
#include "HedgehogEngine/api/ECS/components/CameraComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiTextComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/Engine.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogRenderer/Renderer.hpp"
#include "HedgehogScripting/api/ScriptSystem.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"
#include "HedgehogAudio/api/AudioEngine.hpp"
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
            case ContentType::Font:     return "UI text";
            case ContentType::Audio:    return "Audio source";
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

        // The project's input actions open in their own editor rather than as a plain YAML file.
        if (request.VirtualPath == HedgehogEngine::EngineContext::INPUT_ACTIONS_PATH)
        {
            m_InputActionsWindow->OpenFile(fileSystem);
            return;
        }

        switch (request.Type)
        {
        case ContentType::Scene:
            // As File > Open does, and like it only in Edit: Stop would overwrite the load.
            if (engineContext.GetPlayState() != HedgehogEngine::PlayState::Edit)
            {
                LOGWARNING("Stop play mode before opening a scene (", request.VirtualPath, ").");
                return;
            }
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
        case ContentType::Audio:
            // An editor tool, so it plays in Edit mode too; the clip goes onto a source by dragging.
            PreviewAudio(engineContext, request.VirtualPath);
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
        case ContentType::Font:
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
            if (!AssignScript(context, entity, physicalPath))
                return true; // refused, and said why; nothing to open instead
            break;
        case ContentType::Font:
            if (!ecs.HasComponent<HedgehogEngine::UiTextComponent>(entity))
                return false;
            ecs.GetComponent<HedgehogEngine::UiTextComponent>(entity).Font = relativePath;
            break;
        case ContentType::Audio:
            if (!ecs.HasComponent<HedgehogEngine::AudioSourceComponent>(entity))
                return false;
            ecs.GetComponent<HedgehogEngine::AudioSourceComponent>(entity).Clip = relativePath;
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

    bool EditorGui::AssignScript(HedgehogEngine::Engine& context, ECS::Entity entity, const std::string& physicalPath)
    {
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        if (engineContext.GetPlayState() != HedgehogEngine::PlayState::Edit)
        {
            LOGWARNING("Scripts are assigned in Edit mode; stop playing first.");
            return false;
        }
        if (!m_ScriptSystem || !ecs.HasComponent<HedgehogEngine::ScriptComponent>(entity))
            return false;

        const auto virtualPath = engineContext.GetFileSystem().ToVirtualPath(physicalPath);
        if (!virtualPath || !virtualPath->starts_with(ASSETS_PREFIX))
        {
            LOGERROR("Editor: '", physicalPath, "' is not under assets://, so it cannot be a script.");
            return false;
        }

        // The component keeps the path under assets://, as scenes store it; the properties start
        // at the script's declared defaults, and no script code runs until Play.
        auto& script      = ecs.GetComponent<HedgehogEngine::ScriptComponent>(entity);
        script.ScriptPath = virtualPath->substr(ASSETS_PREFIX.size());
        script.Properties.clear();
        for (const HedgehogScripting::ScriptPropertyDeclaration& declaration : m_ScriptSystem->DescribeScript(*virtualPath))
            script.Properties.push_back(declaration.Default);
        return true;
    }

    void EditorGui::AcceptSelectionDrop(ContentType type)
    {
        if (const auto drop = AcceptAssetDrop({ type }))
            m_AssetDrop = AssetDrop{ *drop, false, std::nullopt };
    }

    void EditorGui::PreviewAudio(HedgehogEngine::EngineContext& engineContext, const std::string& virtualPath)
    {
        HA::AudioEngine& audio = engineContext.GetAudioEngine();
        // Opening the clip that is playing again stops it; any other clip replaces it.
        const bool stopOnly = audio.IsPlaying(m_PreviewSound) && m_PreviewPath == virtualPath;
        audio.Stop(m_PreviewSound);
        m_PreviewSound = {};
        if (stopOnly)
            return;

        // A 2D one-shot at full volume. The engine's AudioSystem reclaims it when it ends, and
        // Stop's StopAll ends it with the scene's sounds.
        const HA::AudioClipId clip = audio.LoadClip(virtualPath, engineContext.GetFileSystem());
        m_PreviewSound             = audio.Play(clip, HA::PlayParams{});
        m_PreviewPath              = virtualPath;
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
        // Only in Edit, like File > Open: Stop would overwrite the load.
        const bool editing = context.GetEngineContext().GetPlayState() == HedgehogEngine::PlayState::Edit;
        if (!editing)
            ImGui::TextUnformatted("Stop play mode to open a scene.");
        if (!editing) ImGui::BeginDisabled();
        if (ImGui::Button("Open"))
        {
            if (m_SceneToOpen)
                OpenContentItem(context, *m_SceneToOpen);
            close = true;
        }
        if (!editing) ImGui::EndDisabled();
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

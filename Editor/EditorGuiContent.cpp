#include "EditorGui.hpp"
#include "Panels/ContentPanel.hpp"
#include "Platform/ShellActions.hpp"
#include "Tools/PipelineWindow.hpp"
#include "Tools/RenderGraphEditor/GraphFileReference.hpp"
#include "Tools/RenderGraphEditor/RenderGraphEditorWindow.hpp"
#include "Tools/ShaderWindow.hpp"
#include "Tools/VertexDescriptionWindow.hpp"

#include "HedgehogEngine/api/Containers/MaterialContainer.hpp"
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
#include "Logger/api/Logger.hpp"

#include <string_view>

namespace Editor
{
    namespace
    {
        constexpr std::string_view ASSETS_PREFIX = "assets://";

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
        default:
            return false;
        }
        // A mesh or material not loaded before gets a new index that the containers only learn of
        // in the next frame's catalog update; the inspector reads it this frame, so sync now.
        engineContext.GetResourceCatalog().Update(*engineContext.GetRenderSystem(), *engineContext.GetMeshSystem());
        LOGINFO("Content: assigned '", request.VirtualPath, "' to the selected entity.");
        return true;
    }
}

#include "EngineComponents.hpp"

#include "HedgehogEngine/api/ECS/components/AnimatorComponent.hpp"
#include "HedgehogEngine/api/ECS/components/AudioListenerComponent.hpp"
#include "HedgehogEngine/api/ECS/components/AudioSourceComponent.hpp"
#include "HedgehogEngine/api/ECS/components/CameraComponent.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/PrefabInstanceComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiButtonComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiCanvasComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiImageComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiRectComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiTextComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/Scene/ScriptComponentSerializer.hpp"

#include "EcsSerialization/api/ComponentTypeRegistry.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <cassert>

namespace HedgehogEngine
{
    namespace
    {
        using EcsSerialization::ComponentDesc;

        constexpr const char* UI_CATEGORY    = "UI";
        constexpr const char* AUDIO_CATEGORY = "Audio";

        // The default cube, resolved at once so the mesh shows without waiting for a scene refresh.
        void AddDefaultMesh(ECS::ECS& ecs, ECS::Entity entity)
        {
            ecs.AddComponent(entity, MeshComponent{ MeshSystem::sDefaultMeshPath });
            const auto                   meshes = ecs.GetSystem<MeshSystem>();
            const FS::FileSystemManager* files  = ecs.GetServices().Find<FS::FileSystemManager>();
            if (meshes && files)
                meshes->Update(ecs, entity, *files);
        }

        void AddDefaultRender(ECS::ECS& ecs, ECS::Entity entity)
        {
            ecs.AddComponent(entity, RenderComponent{});
            if (const auto renders = ecs.GetSystem<RenderSystem>())
                renders->Update(ecs, entity);
        }
    }

    void RegisterEngineComponents(EcsSerialization::ComponentTypeRegistry& types)
    {
        bool registered = true;
        registered &= types.RegisterReflected<TransformComponent>(
            ComponentDesc{ .Key = "TransformComponent", .DisplayName = "Transform", .Icon = "transform", .Addable = false, .Removable = false });
        // Written by the scene serializer itself, through the hierarchy provider.
        registered &= types.RegisterUnserialized<ECS::HierarchyComponent>(
            ComponentDesc{ .Key = "HierarchyComponent", .DisplayName = "Hierarchy", .Addable = false, .Removable = false });
        registered &= types.RegisterReflected<MeshComponent>(
            ComponentDesc{ .Key = "MeshComponent", .DisplayName = "Mesh component", .Icon = "mesh", .AddDefault = AddDefaultMesh });
        registered &= types.RegisterReflected<RenderComponent>(ComponentDesc{
            .Key = "RenderComponent", .DisplayName = "Render component", .Icon = "material", .EnabledProperty = "Visible", .AddDefault = AddDefaultRender });
        registered &= types.RegisterReflected<LightComponent>(
            ComponentDesc{ .Key = "LightComponent", .DisplayName = "Light component", .Icon = "light", .EnabledProperty = "LightEnabled" });
        registered &= types.RegisterReflected<CameraComponent>(
            ComponentDesc{ .Key = "CameraComponent", .DisplayName = "Camera component", .Icon = "camera", .EnabledProperty = "Enabled" });
        registered &= types.RegisterReflected<AnimatorComponent>(
            ComponentDesc{ .Key = "AnimatorComponent", .DisplayName = "Animator component", .Icon = "animator" });
        registered &= types.RegisterReflected<UiCanvasComponent>(ComponentDesc{
            .Key = "UiCanvasComponent", .DisplayName = "UI canvas component", .Category = UI_CATEGORY, .Icon = "ui_canvas", .EnabledProperty = "Enabled" });
        registered &= types.RegisterReflected<UiRectComponent>(
            ComponentDesc{ .Key = "UiRectComponent", .DisplayName = "UI rect component", .Category = UI_CATEGORY, .Icon = "ui_rect" });
        registered &= types.RegisterReflected<UiImageComponent>(
            ComponentDesc{ .Key = "UiImageComponent", .DisplayName = "UI image component", .Category = UI_CATEGORY, .Icon = "ui_image" });
        registered &= types.RegisterReflected<UiTextComponent>(
            ComponentDesc{ .Key = "UiTextComponent", .DisplayName = "UI text component", .Category = UI_CATEGORY, .Icon = "ui_text" });
        registered &= types.RegisterReflected<UiButtonComponent>(
            ComponentDesc{ .Key = "UiButtonComponent", .DisplayName = "UI button component", .Category = UI_CATEGORY, .Icon = "ui_button" });
        registered &= types.RegisterReflected<AudioSourceComponent>(ComponentDesc{
            .Key = "AudioSourceComponent", .DisplayName = "Audio source component", .Category = AUDIO_CATEGORY, .Icon = "audio_source" });
        registered &= types.RegisterReflected<AudioListenerComponent>(
            ComponentDesc{ .Key           = "AudioListenerComponent",
                           .DisplayName   = "Audio listener component",
                           .Category      = AUDIO_CATEGORY,
                           .Icon          = "audio_listener",
                           .EnabledProperty = "Active" });
        registered &= types.RegisterReflected<PrefabInstanceComponent>(
            ComponentDesc{ .Key = "PrefabInstanceComponent", .DisplayName = "Prefab instance", .Addable = false, .Removable = false });
        // Scripts run in the application's script system; loading a scene only reads the data.
        registered &= types.RegisterCustom<ScriptComponent>(
            ComponentDesc{ .Key = "ScriptComponent", .DisplayName = "Script component", .Icon = "script" }, RegisterScriptComponentSerializer);
        assert(registered && "RegisterEngineComponents: an engine component type was refused.");
        (void)registered;
    }
}

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
#include "EcsSerialization/api/UnknownComponents.hpp"

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
        // SortOrder is the inspector's order (Transform, Light, Camera, Mesh, Rendering, Script,
        // Animator, then the UI and audio sections), which the Add Component menu follows too.
        bool registered = true;
        registered &= types.RegisterReflected<TransformComponent>(ComponentDesc{
            .Key = "TransformComponent", .DisplayName = "Transform", .SortOrder = 0, .Icon = "transform", .Addable = false, .Removable = false });
        // Written by the scene serializer itself, through the hierarchy provider.
        registered &= types.RegisterUnserialized<ECS::HierarchyComponent>(ComponentDesc{
            .Key = "HierarchyComponent", .DisplayName = "Hierarchy", .Addable = false, .Removable = false, .Inspectable = false });
        registered &= types.RegisterReflected<MeshComponent>(
            ComponentDesc{ .Key = "MeshComponent", .DisplayName = "Mesh", .SortOrder = 30, .Icon = "mesh", .AddDefault = AddDefaultMesh });
        registered &= types.RegisterReflected<RenderComponent>(ComponentDesc{ .Key             = "RenderComponent",
                                                                              .DisplayName     = "Render",
                                                                              .SortOrder       = 40,
                                                                              .Icon            = "material",
                                                                              .EnabledProperty = "Visible",
                                                                              .AddDefault      = AddDefaultRender });
        registered &= types.RegisterReflected<LightComponent>(ComponentDesc{
            .Key = "LightComponent", .DisplayName = "Light", .SortOrder = 10, .Icon = "light", .EnabledProperty = "LightEnabled" });
        registered &= types.RegisterReflected<CameraComponent>(ComponentDesc{
            .Key = "CameraComponent", .DisplayName = "Camera", .SortOrder = 20, .Icon = "camera", .EnabledProperty = "Enabled" });
        registered &= types.RegisterReflected<AnimatorComponent>(
            ComponentDesc{ .Key = "AnimatorComponent", .DisplayName = "Animator", .SortOrder = 60, .Icon = "animator" });
        registered &= types.RegisterReflected<UiCanvasComponent>(ComponentDesc{ .Key             = "UiCanvasComponent",
                                                                                .DisplayName     = "UI canvas",
                                                                                .Category        = UI_CATEGORY,
                                                                                .SortOrder       = 100,
                                                                                .Icon            = "ui_canvas",
                                                                                .EnabledProperty = "Enabled" });
        registered &= types.RegisterReflected<UiRectComponent>(ComponentDesc{
            .Key = "UiRectComponent", .DisplayName = "UI rect", .Category = UI_CATEGORY, .SortOrder = 101, .Icon = "ui_rect" });
        registered &= types.RegisterReflected<UiImageComponent>(ComponentDesc{
            .Key = "UiImageComponent", .DisplayName = "UI image", .Category = UI_CATEGORY, .SortOrder = 102, .Icon = "ui_image" });
        registered &= types.RegisterReflected<UiTextComponent>(ComponentDesc{
            .Key = "UiTextComponent", .DisplayName = "UI text", .Category = UI_CATEGORY, .SortOrder = 103, .Icon = "ui_text" });
        registered &= types.RegisterReflected<UiButtonComponent>(ComponentDesc{
            .Key = "UiButtonComponent", .DisplayName = "UI button", .Category = UI_CATEGORY, .SortOrder = 104, .Icon = "ui_button" });
        registered &= types.RegisterReflected<AudioSourceComponent>(ComponentDesc{
            .Key = "AudioSourceComponent", .DisplayName = "Audio source", .Category = AUDIO_CATEGORY, .SortOrder = 200, .Icon = "audio_source" });
        registered &= types.RegisterReflected<AudioListenerComponent>(ComponentDesc{ .Key             = "AudioListenerComponent",
                                                                                     .DisplayName     = "Audio listener",
                                                                                     .Category        = AUDIO_CATEGORY,
                                                                                     .SortOrder       = 201,
                                                                                     .Icon            = "audio_listener",
                                                                                     .EnabledProperty = "Active" });
        registered &= types.RegisterReflected<PrefabInstanceComponent>(ComponentDesc{ .Key         = "PrefabInstanceComponent",
                                                                                      .DisplayName = "Prefab instance",
                                                                                      .Addable     = false,
                                                                                      .Removable   = false,
                                                                                      .Inspectable = false });
        // Scripts run in the application's script system; loading a scene only reads the data.
        registered &= types.RegisterCustom<ScriptComponent>(
            ComponentDesc{ .Key = "ScriptComponent", .DisplayName = "Script", .SortOrder = 50, .Icon = "script" },
            RegisterScriptComponentSerializer);
        // Keeps the data of components no registered type reads (a plugin that is not loaded), so
        // scenes, snapshots and saves write it back unchanged. The scene serializer writes it.
        registered &= types.RegisterUnserialized<EcsSerialization::UnknownComponentsComponent>(
            ComponentDesc{ .Key         = EcsSerialization::UNKNOWN_COMPONENTS_KEY,
                           .DisplayName = "Unknown components",
                           .Addable     = false,
                           .Removable   = false,
                           .Inspectable = false });
        assert(registered && "RegisterEngineComponents: an engine component type was refused.");
        (void)registered;
    }
}

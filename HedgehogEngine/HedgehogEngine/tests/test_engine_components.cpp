#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "EcsSerialization/api/ComponentSerializerRegistry.hpp"
#include "EcsSerialization/api/ComponentTypeRegistry.hpp"

#include "ECS/api/ECS.hpp"

#include <set>
#include <string>
#include <vector>

using namespace HedgehogEngine;
using EcsSerialization::ComponentInfo;

TEST_CASE("Engine components - all 20 are registered once, in serializer order, under their YAML keys")
{
    EngineContext                                  context;
    const EcsSerialization::ComponentTypeRegistry& types = context.GetComponentTypes();
    CHECK(context.GetECS().GetServices().Find<EcsSerialization::ComponentTypeRegistry>() == &context.GetComponentTypes());

    std::vector<std::string> keys;
    for (const ComponentInfo& info : types.GetInfos())
        keys.push_back(info.Key);
    const std::vector<std::string> expected = {
        "TransformComponent", "HierarchyComponent", "MeshComponent",     "RenderComponent",       "LightComponent",
        "CameraComponent",    "AnimatorComponent",  "UiCanvasComponent", "UiRectComponent",       "UiImageComponent",
        "UiTextComponent",    "UiButtonComponent",  "AudioSourceComponent", "AudioListenerComponent",
        "RigidBodyComponent", "ColliderComponent", "EnvironmentComponent", "PrefabInstanceComponent", "ScriptComponent",
        "UnknownComponents",
    };
    CHECK(keys == expected);

    // Every key but the hierarchy's and the unknown store's (the scene serializer writes both) is a
    // serializer's YAML key, and the serializers write in the same order.
    std::vector<std::string> handlerKeys;
    for (const EcsSerialization::ComponentHandler& handler : context.GetComponentRegistry().GetHandlers())
        handlerKeys.push_back(handler.YamlKey);
    std::vector<std::string> serialized = expected;
    std::erase(serialized, "HierarchyComponent");
    std::erase(serialized, "UnknownComponents");
    CHECK(handlerKeys == serialized);

    std::set<std::string> addable;
    for (const ComponentInfo& info : types.GetInfos())
    {
        if (info.Addable)
            addable.insert(info.Key);
    }
    CHECK(addable.size() == 16);
    CHECK_FALSE(types.Find("TransformComponent")->Addable);
    CHECK_FALSE(types.Find("TransformComponent")->Removable);
    CHECK_FALSE(types.Find("PrefabInstanceComponent")->Addable);
    CHECK(types.Find("UiTextComponent")->Category == "UI");
    CHECK(types.Find("AudioSourceComponent")->Category == "Audio");
    CHECK(types.Find("LightComponent")->Category.empty());
    CHECK(types.Find("RigidBodyComponent")->Category == "Physics");
    CHECK(types.Find("ColliderComponent")->Category == "Physics");
    CHECK(types.Find("RigidBodyComponent")->Removable);
    CHECK(types.Find("ColliderComponent")->Icon == "collider");
    CHECK(types.Find("RigidBodyComponent")->Icon == "rigid_body");
    CHECK(types.Find("EnvironmentComponent")->Category == "Rendering");
    CHECK(types.Find("EnvironmentComponent")->Icon == "scene");
    CHECK(types.Find("EnvironmentComponent")->EnabledProperty == "Enabled");
}

TEST_CASE("Engine components - AddDefault gives what the editor menu adds")
{
    EngineContext                                  context;
    ECS::ECS&                                      ecs    = context.GetECS();
    const EcsSerialization::ComponentTypeRegistry& types  = context.GetComponentTypes();
    const ECS::Entity                              entity = context.GetSceneManager().CreateGameObject();

    // A mesh resolves to the default cube at once.
    types.Find("MeshComponent")->AddDefault(ecs, entity);
    const MeshComponent& mesh = ecs.GetComponent<MeshComponent>(entity);
    CHECK(mesh.MeshPath == MeshSystem::sDefaultMeshPath);
    REQUIRE(mesh.MeshIndex.has_value());
    CHECK(context.GetMeshSystem()->GetMeshes()[*mesh.MeshIndex] == MeshSystem::sDefaultMeshPath);

    // A render component has no material until one is set, then an index.
    const ComponentInfo& render = *types.Find("RenderComponent");
    render.AddDefault(ecs, entity);
    REQUIRE(render.Has(ecs, entity));
    CHECK_FALSE(ecs.GetComponent<RenderComponent>(entity).MaterialIndex.has_value());
    ecs.GetComponent<RenderComponent>(entity).Material = "Materials/test1.material";
    context.GetRenderSystem()->Update(ecs, entity);
    CHECK(ecs.GetComponent<RenderComponent>(entity).MaterialIndex.has_value());

    // Its header checkbox is the component's visibility.
    bool* visible = render.Enabled(render.Get(ecs, entity));
    REQUIRE(visible != nullptr);
    CHECK(visible == &ecs.GetComponent<RenderComponent>(entity).IsVisible);

    // The others add a default component.
    const ComponentInfo& light = *types.Find("LightComponent");
    light.AddDefault(ecs, entity);
    CHECK(ecs.HasComponent<LightComponent>(entity));
    CHECK(light.Enabled(light.Get(ecs, entity)) == &ecs.GetComponent<LightComponent>(entity).Enable);
    for (const ComponentInfo& info : types.GetInfos())
    {
        if (!info.Addable || info.Has(ecs, entity))
            continue;
        info.AddDefault(ecs, entity);
        CHECK_MESSAGE(info.Has(ecs, entity), info.Key);
    }
}

TEST_CASE("Engine components - the inspector order, and what the inspector draws")
{
    EngineContext context;

    std::vector<std::string> shown;
    for (const ComponentInfo* info : context.GetComponentTypes().GetInfosInOrder())
    {
        if (info->Inspectable)
            shown.push_back(info->DisplayName);
    }
    const std::vector<std::string> expected = {
        "Transform", "Light",   "Camera",    "Mesh",    "Render",       "Script",         "Animator", "UI canvas",
        "UI rect",   "UI image", "UI text",  "UI button", "Audio source", "Audio listener", "Rigid body", "Collider",
        "Environment",
    };
    CHECK(shown == expected);
    CHECK_FALSE(context.GetComponentTypes().Find("HierarchyComponent")->Inspectable);
    CHECK_FALSE(context.GetComponentTypes().Find("PrefabInstanceComponent")->Inspectable);
}

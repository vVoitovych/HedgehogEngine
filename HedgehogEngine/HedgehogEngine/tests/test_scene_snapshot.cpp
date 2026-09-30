#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include <algorithm>

using HedgehogEngine::EngineContext;
using HedgehogEngine::SceneManager;
using HedgehogEngine::SceneSnapshot;
using HedgehogEngine::TransformComponent;

namespace
{
    const std::string& NameOf(ECS::ECS& ecs, ECS::Entity entity)
    {
        return ecs.GetComponent<ECS::HierarchyComponent>(entity).Name;
    }

    bool HasChild(ECS::ECS& ecs, ECS::Entity parent, ECS::Entity child)
    {
        const auto& children = ecs.GetComponent<ECS::HierarchyComponent>(parent).Children;
        return std::find(children.begin(), children.end(), child) != children.end();
    }
}

TEST_CASE("SceneManager snapshot - capture, mutate, restore gives back the captured scene")
{
    EngineContext context;
    SceneManager& scenes = context.GetSceneManager();
    ECS::ECS&     ecs    = context.GetECS();

    // root -> a -> b, root -> c
    const ECS::Entity root = scenes.GetRootEntity();
    const ECS::Entity a    = scenes.CreateGameObject();
    const ECS::Entity b    = scenes.CreateGameObject(a);
    const ECS::Entity c    = scenes.CreateGameObject();
    ecs.GetComponent<TransformComponent>(a).Position = HM::Vector3(1.0f, 2.0f, 3.0f);
    ecs.GetComponent<TransformComponent>(b).Scale    = HM::Vector3(0.5f, 0.25f, 2.0f);
    scenes.SetSceneName("Snapshotted");

    const std::string nameA = NameOf(ecs, a);
    const std::string nameB = NameOf(ecs, b);
    const std::string nameC = NameOf(ecs, c);

    const SceneSnapshot snapshot = scenes.CaptureSnapshot();
    CHECK(snapshot.SceneName == "Snapshotted");
    CHECK(snapshot.GameObjectIndex == 3u);
    CHECK(snapshot.Yaml.find(nameA) != std::string::npos);

    // Mutate: move a transform, delete an entity, add one, rename the scene.
    ecs.GetComponent<TransformComponent>(a).Position = HM::Vector3(-9.0f, 0.0f, 9.0f);
    scenes.DeleteGameObject(c);
    const ECS::Entity added = scenes.CreateGameObject(b);
    scenes.SetSceneName("Mutated");
    REQUIRE(scenes.CaptureSnapshot().Yaml != snapshot.Yaml);

    REQUIRE(scenes.RestoreSnapshot(snapshot));

    const SceneSnapshot restored = scenes.CaptureSnapshot();
    CHECK(restored.Yaml == snapshot.Yaml);
    CHECK(restored.SceneName == "Snapshotted");
    CHECK(restored.GameObjectIndex == 3u);
    CHECK(scenes.GetSceneName() == "Snapshotted");

    // Every captured entity is back under its old id, in its old place.
    CHECK(scenes.GetRootEntity() == root);
    REQUIRE(ecs.IsAlive(a));
    REQUIRE(ecs.IsAlive(b));
    REQUIRE(ecs.IsAlive(c));
    CHECK(NameOf(ecs, a) == nameA);
    CHECK(NameOf(ecs, b) == nameB);
    CHECK(NameOf(ecs, c) == nameC);
    CHECK(HasChild(ecs, root, a));
    CHECK(HasChild(ecs, a, b));
    CHECK(HasChild(ecs, root, c));
    CHECK(ecs.GetComponent<TransformComponent>(a).Position.x() == 1.0f);
    CHECK(ecs.GetComponent<TransformComponent>(b).Scale.y() == 0.25f);

    // The entity added after the capture is gone, unless its id was reused for c.
    if (added != c)
        CHECK_FALSE(ecs.IsAlive(added));
    CHECK(ecs.GetComponent<ECS::HierarchyComponent>(b).Children.empty());
}

TEST_CASE("SceneManager snapshot - the game-object index carries on from the snapshot")
{
    EngineContext context;
    SceneManager& scenes = context.GetSceneManager();
    ECS::ECS&     ecs    = context.GetECS();

    (void)scenes.CreateGameObject();
    const SceneSnapshot snapshot = scenes.CaptureSnapshot();

    (void)scenes.CreateGameObject();
    (void)scenes.CreateGameObject();
    REQUIRE(scenes.RestoreSnapshot(snapshot));

    // The next name continues from the captured index, so it cannot clash with
    // a name the restored scene already holds.
    const ECS::Entity next = scenes.CreateGameObject();
    CHECK(NameOf(ecs, next) == "GameObject_1");
}

TEST_CASE("SceneManager snapshot - a snapshot restores repeatedly")
{
    EngineContext context;
    SceneManager& scenes = context.GetSceneManager();

    (void)scenes.CreateGameObject();
    const SceneSnapshot snapshot = scenes.CaptureSnapshot();

    for (int i = 0; i < 3; ++i)
    {
        (void)scenes.CreateGameObject();
        REQUIRE(scenes.RestoreSnapshot(snapshot));
        CHECK(scenes.CaptureSnapshot().Yaml == snapshot.Yaml);
    }
}

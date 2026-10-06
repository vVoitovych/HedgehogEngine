#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/HierarchySystem.hpp"
#include "HedgehogEngine/api/ECS/systems/TransformSystem.hpp"
#include "HedgehogEngine/api/Save/SaveGameManager.hpp"

#include "EcsSerialization/api/UnknownComponents.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "FileSystem/tests/test_helpers.hpp"

#include "yaml-cpp/yaml.h"

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

TEST_CASE("SceneManager snapshot - a malformed snapshot returns false")
{
    EngineContext context;
    SceneManager& scenes = context.GetSceneManager();

    SceneSnapshot broken;
    broken.Yaml      = "Scene name: [unclosed";
    broken.SceneName = "Broken";
    CHECK_FALSE(scenes.RestoreSnapshot(broken));
}

TEST_CASE("SceneManager - a game object created and deleted in one frame is skipped by the transform update")
{
    EngineContext context;
    SceneManager& scenes = context.GetSceneManager();
    ECS::ECS&     ecs    = context.GetECS();

    // Both publish a transform change; the deleted one must not be read when it is processed.
    const ECS::Entity kept    = scenes.CreateGameObject();
    const ECS::Entity deleted = scenes.CreateGameObject();
    ecs.GetComponent<TransformComponent>(kept).Position = HM::Vector3(1.0f, 2.0f, 3.0f);
    scenes.DeleteGameObject(deleted);

    context.GetTransformSystem()->Update(ecs, context.GetEventBus());
    context.GetHierarchySystem()->Update(ecs, context.GetEventBus());

    CHECK_FALSE(ecs.IsAlive(deleted));
    CHECK(ecs.GetComponent<TransformComponent>(kept).ObjMatrix[3].x() == doctest::Approx(1.0f));
}

TEST_CASE("Unknown components - kept through Play and Stop and through a save game")
{
    using EcsSerialization::UnknownComponent;
    using EcsSerialization::UnknownComponentsComponent;

    TempDir       saves;
    EngineContext context;
    context.GetSaveGames().SetSaveDirectory(saves.Path());
    ECS::ECS&         ecs    = context.GetECS();
    const ECS::Entity entity = context.GetSceneManager().CreateGameObject();
    ecs.AddComponent(entity, UnknownComponentsComponent{ { UnknownComponent{ "SpinnerComponent", "{Speed: 90}" } } });

    const auto speedOf = [&]
    {
        REQUIRE(ecs.HasComponent<UnknownComponentsComponent>(entity));
        const auto& entries = ecs.GetComponent<UnknownComponentsComponent>(entity).Entries;
        REQUIRE(entries.size() == 1);
        CHECK(entries[0].Key == "SpinnerComponent");
        return YAML::Load(entries[0].Yaml)["Speed"].as<int>();
    };

    // Play's snapshot and Stop's restore.
    REQUIRE(context.Play());
    ecs.GetComponent<UnknownComponentsComponent>(entity).Entries.clear();
    REQUIRE(context.Stop());
    CHECK(speedOf() == 90);

    // A save game's World section, loaded over a world that lost the data.
    REQUIRE(context.Play());
    REQUIRE(context.GetSaveGames().RequestSave("slot"));
    context.UpdatePlayMode(1.0f / 60.0f);
    ecs.RemoveComponent<UnknownComponentsComponent>(entity);
    REQUIRE(context.GetSaveGames().RequestLoad("slot"));
    context.UpdatePlayMode(1.0f / 60.0f);
    CHECK(speedOf() == 90);
    REQUIRE(context.Stop());
}

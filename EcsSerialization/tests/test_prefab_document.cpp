#include "doctest/doctest/doctest.h"

#include "EcsSerialization/api/ComponentSerializerRegistry.hpp"
#include "EcsSerialization/api/EcsSerializer.hpp"
#include "EcsSerialization/api/Prefab/PrefabDocument.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include <string>

namespace
{
    struct TestScale
    {
        float Value = 1.0f;

        template<typename Visitor>
        void Visit(Visitor& visitor)
        {
            visitor("Value", Value);
        }
    };
}

TEST_CASE("PrefabDocument - a written prefab reads back as its subtree and instantiates")
{
    ECS::ECS ecs;
    ecs.Init();
    ecs.RegisterComponent<ECS::HierarchyComponent>();
    ecs.RegisterComponent<TestScale>();
    EcsSerialization::ComponentSerializerRegistry registry;
    registry.RegisterVisitable<TestScale>("TestScale");

    const ECS::Entity root = ecs.CreateEntity();
    ecs.AddComponent(root, ECS::HierarchyComponent{ "Root", ECS::INVALID_ENTITY, {} });
    ecs.SetRoot(root);
    const ECS::Entity crate = ecs.CreateEntity();
    ecs.AddComponent(crate, ECS::HierarchyComponent{ "Crate", root, {} });
    ecs.AddComponent(crate, TestScale{ 3.0f });
    ecs.GetComponent<ECS::HierarchyComponent>(root).Children.push_back(crate);

    const std::string text = EcsSerialization::WritePrefab(registry, ecs, crate);
    CHECK(text.starts_with("Version: 1\nRoot:"));

    const EcsSerialization::PrefabReadResult read = EcsSerialization::ReadPrefab(text);
    REQUIRE(read.Root);
    CHECK(read.Error.empty());
    CHECK((*read.Root)["Subtree"][0]["Name"].as<std::string>() == "Crate");

    const ECS::Entity copy = EcsSerialization::EcsSerializer::InstantiateSubtree(registry, ecs, *read.Root, root, "Crate.prefab");
    REQUIRE(copy != ECS::INVALID_ENTITY);
    CHECK(ecs.GetComponent<TestScale>(copy).Value == 3.0f);
    CHECK(ecs.GetComponent<ECS::HierarchyComponent>(copy).Name == "Crate");
}

TEST_CASE("PrefabDocument - malformed prefabs are refused with a reason")
{
    const struct
    {
        const char* Text;
        const char* Error;
    } cases[] = {
        { "Version: [1", "not valid YAML" },
        { "- 1\n- 2\n", "not a prefab document" },
        { "Root: {Subtree: []}\n", "Version is missing" },
        { "Version: 0\nRoot: {Subtree: []}\n", "Version is missing or is not a positive integer" },
        { "Version: 2\nRoot: {Subtree: []}\n", "the prefab is format version 2, but this build reads up to version 1" },
        { "Version: 1\n", "Root is missing" },
        { "Version: 1\nRoot: {Subtree: 3}\n", "Root is missing or holds no Subtree" },
    };
    for (const auto& c : cases)
    {
        CAPTURE(c.Text);
        const EcsSerialization::PrefabReadResult read = EcsSerialization::ReadPrefab(c.Text);
        CHECK_FALSE(read.Root);
        CHECK(read.Error.find(c.Error) != std::string::npos);
    }
}

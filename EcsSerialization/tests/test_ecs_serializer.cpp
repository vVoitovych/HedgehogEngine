#include "doctest/doctest/doctest.h"

#include "EcsSerialization/api/EcsSerializer.hpp"
#include "EcsSerialization/api/ComponentSerializerRegistry.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include "HedgehogMath/api/Vector.hpp"

namespace
{
    struct TestTransform
    {
        HM::Vector3 m_Position{};
        float       m_Scale{ 1.0f };

        template<typename Visitor>
        void Visit(Visitor& visitor)
        {
            visitor("Position", m_Position);
            visitor("Scale",    m_Scale);
        }
    };

    // Mounts "scene://" at the given directory.
    void MountScene(FS::FileSystemManager& manager, const std::filesystem::path& dir)
    {
        auto fs = std::make_unique<FS::FileSystem>();
        fs->RegisterPath("scene://", dir);
        REQUIRE(manager.Register(std::move(fs)));
    }

    // Fresh ECS with the components the serializer needs.
    ECS::ECS MakeEcs()
    {
        ECS::ECS ecs;
        ecs.Init();
        ecs.RegisterComponent<ECS::HierarchyComponent>();
        ecs.RegisterComponent<TestTransform>();
        return ecs;
    }
}

TEST_CASE("EcsSerializer - scene round-trips through YAML")
{
    TempDir tmp;
    FS::FileSystemManager fileSystem;
    MountScene(fileSystem, tmp.Path());

    EcsSerialization::ComponentSerializerRegistry registry;
    registry.RegisterVisitable<TestTransform>("TestTransform");

    // Source scene: root with one child carrying a TestTransform.
    ECS::ECS source = MakeEcs();

    const ECS::Entity root = source.CreateEntity();
    source.AddComponent(root, ECS::HierarchyComponent{ "Root", ECS::INVALID_ENTITY, {} });
    source.SetRoot(root);

    const ECS::Entity child = source.CreateEntity();
    source.AddComponent(child, ECS::HierarchyComponent{ "Child", root, {} });
    source.GetComponent<ECS::HierarchyComponent>(root).m_Children.push_back(child);

    TestTransform transform;
    transform.m_Position.x() = 1.0f;
    transform.m_Position.y() = 2.0f;
    transform.m_Position.z() = 3.0f;
    transform.m_Scale        = 0.5f;
    source.AddComponent(child, transform);

    REQUIRE(EcsSerialization::EcsSerializer::Serialize(
        registry, source, "TestScene", "scene://test.yaml", fileSystem));

    // Deserialize into a fresh ECS and compare.
    ECS::ECS target = MakeEcs();
    std::string sceneName;
    REQUIRE(EcsSerialization::EcsSerializer::Deserialize(
        registry, target, sceneName, "scene://test.yaml", fileSystem));

    CHECK(sceneName == "TestScene");
    REQUIRE(target.GetRoot() == root);

    const auto& rootHierarchy = target.GetComponent<ECS::HierarchyComponent>(root);
    CHECK(rootHierarchy.m_Name == "Root");
    REQUIRE(rootHierarchy.m_Children.size() == 1u);
    CHECK(rootHierarchy.m_Children.front() == child);

    const auto& childHierarchy = target.GetComponent<ECS::HierarchyComponent>(child);
    CHECK(childHierarchy.m_Name == "Child");
    CHECK(childHierarchy.m_Parent == root);

    REQUIRE(target.HasComponent<TestTransform>(child));
    const auto& loaded = target.GetComponent<TestTransform>(child);
    CHECK(loaded.m_Position.x() == 1.0f);
    CHECK(loaded.m_Position.y() == 2.0f);
    CHECK(loaded.m_Position.z() == 3.0f);
    CHECK(loaded.m_Scale == 0.5f);
}

TEST_CASE("EcsSerializer::Serialize - unmounted path returns false")
{
    TempDir tmp;
    FS::FileSystemManager fileSystem;
    MountScene(fileSystem, tmp.Path());

    EcsSerialization::ComponentSerializerRegistry registry;
    ECS::ECS ecs = MakeEcs();

    const ECS::Entity root = ecs.CreateEntity();
    ecs.AddComponent(root, ECS::HierarchyComponent{ "Root", ECS::INVALID_ENTITY, {} });
    ecs.SetRoot(root);

    CHECK_FALSE(EcsSerialization::EcsSerializer::Serialize(
        registry, ecs, "TestScene", "unknown://test.yaml", fileSystem));
}

TEST_CASE("EcsSerializer::Deserialize - missing file returns false")
{
    TempDir tmp;
    FS::FileSystemManager fileSystem;
    MountScene(fileSystem, tmp.Path());

    EcsSerialization::ComponentSerializerRegistry registry;
    ECS::ECS ecs = MakeEcs();
    std::string sceneName;

    CHECK_FALSE(EcsSerialization::EcsSerializer::Deserialize(
        registry, ecs, sceneName, "scene://does_not_exist.yaml", fileSystem));
}

TEST_CASE("EcsSerializer::Deserialize - malformed YAML returns false")
{
    TempDir tmp;
    FS::FileSystemManager fileSystem;
    MountScene(fileSystem, tmp.Path());
    tmp.WriteFile("broken.yaml", "Scene name: [unclosed");

    EcsSerialization::ComponentSerializerRegistry registry;
    ECS::ECS ecs = MakeEcs();
    std::string sceneName;

    CHECK_FALSE(EcsSerialization::EcsSerializer::Deserialize(
        registry, ecs, sceneName, "scene://broken.yaml", fileSystem));
}

TEST_CASE("EcsSerializer::Deserialize - valid YAML without scene keys returns false")
{
    TempDir tmp;
    FS::FileSystemManager fileSystem;
    MountScene(fileSystem, tmp.Path());
    tmp.WriteFile("not_a_scene.yaml", "just_some_key: 5\n");

    EcsSerialization::ComponentSerializerRegistry registry;
    ECS::ECS ecs = MakeEcs();
    std::string sceneName;

    CHECK_FALSE(EcsSerialization::EcsSerializer::Deserialize(
        registry, ecs, sceneName, "scene://not_a_scene.yaml", fileSystem));
}

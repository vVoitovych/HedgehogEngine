#include "doctest/doctest/doctest.h"

#include "EcsSerialization/api/EcsSerializer.hpp"
#include "EcsSerialization/api/ComponentSerializerRegistry.hpp"

#include "yaml-cpp/yaml.h"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <string>

namespace
{
    struct TestTransform
    {
        HM::Vector3 Position{};
        float       Scale{ 1.0f };

        template<typename Visitor>
        void Visit(Visitor& visitor)
        {
            visitor("Position", Position);
            visitor("Scale",    Scale);
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
    source.GetComponent<ECS::HierarchyComponent>(root).Children.push_back(child);

    TestTransform transform;
    transform.Position.x() = 1.0f;
    transform.Position.y() = 2.0f;
    transform.Position.z() = 3.0f;
    transform.Scale        = 0.5f;
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
    CHECK(rootHierarchy.Name == "Root");
    REQUIRE(rootHierarchy.Children.size() == 1u);
    CHECK(rootHierarchy.Children.front() == child);

    const auto& childHierarchy = target.GetComponent<ECS::HierarchyComponent>(child);
    CHECK(childHierarchy.Name == "Child");
    CHECK(childHierarchy.Parent == root);

    REQUIRE(target.HasComponent<TestTransform>(child));
    const auto& loaded = target.GetComponent<TestTransform>(child);
    CHECK(loaded.Position.x() == 1.0f);
    CHECK(loaded.Position.y() == 2.0f);
    CHECK(loaded.Position.z() == 3.0f);
    CHECK(loaded.Scale == 0.5f);
}

namespace
{
    // Root with one child carrying a TestTransform.
    void BuildScene(ECS::ECS& ecs)
    {
        const ECS::Entity root = ecs.CreateEntity();
        ecs.AddComponent(root, ECS::HierarchyComponent{ "Root", ECS::INVALID_ENTITY, {} });
        ecs.SetRoot(root);

        const ECS::Entity child = ecs.CreateEntity();
        ecs.AddComponent(child, ECS::HierarchyComponent{ "Child", root, {} });
        ecs.GetComponent<ECS::HierarchyComponent>(root).Children.push_back(child);

        TestTransform transform;
        transform.Position.x() = 0.1f;
        transform.Scale        = 1.0f / 3.0f;
        ecs.AddComponent(child, transform);
    }
}

TEST_CASE("EcsSerializer::SerializeToString - round-trips byte for byte through a string")
{
    EcsSerialization::ComponentSerializerRegistry registry;
    registry.RegisterVisitable<TestTransform>("TestTransform");

    ECS::ECS source = MakeEcs();
    BuildScene(source);
    const std::string text = EcsSerialization::EcsSerializer::SerializeToString(registry, source, "InMemory");

    ECS::ECS    target = MakeEcs();
    std::string sceneName;
    REQUIRE(EcsSerialization::EcsSerializer::DeserializeFromString(registry, target, sceneName, text, "test"));
    CHECK(sceneName == "InMemory");
    CHECK(target.GetRoot() == source.GetRoot());

    // Floats that are not exact in decimal must survive too.
    CHECK(EcsSerialization::EcsSerializer::SerializeToString(registry, target, sceneName) == text);
}

TEST_CASE("EcsSerializer::Serialize - the file holds exactly SerializeToString's text")
{
    TempDir tmp;
    FS::FileSystemManager fileSystem;
    MountScene(fileSystem, tmp.Path());

    EcsSerialization::ComponentSerializerRegistry registry;
    registry.RegisterVisitable<TestTransform>("TestTransform");

    ECS::ECS ecs = MakeEcs();
    BuildScene(ecs);

    REQUIRE(EcsSerialization::EcsSerializer::Serialize(registry, ecs, "OnDisk", "scene://same.yaml", fileSystem));
    const auto written = fileSystem.ReadTextFile("scene://same.yaml");
    REQUIRE(written.has_value());
    CHECK(*written == EcsSerialization::EcsSerializer::SerializeToString(registry, ecs, "OnDisk"));
}

TEST_CASE("EcsSerializer::DeserializeFromString - malformed or scene-less text returns false")
{
    EcsSerialization::ComponentSerializerRegistry registry;
    std::string sceneName;

    ECS::ECS broken = MakeEcs();
    CHECK_FALSE(EcsSerialization::EcsSerializer::DeserializeFromString(
        registry, broken, sceneName, "Scene name: [unclosed", "broken"));

    ECS::ECS notAScene = MakeEcs();
    CHECK_FALSE(EcsSerialization::EcsSerializer::DeserializeFromString(
        registry, notAScene, sceneName, "just_some_key: 5\n", "not a scene"));
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

namespace
{
    // An ECS untouched by a refused load: no entity was created.
    bool IsEmpty(const ECS::ECS& ecs)
    {
        for (ECS::Entity entity = 0; entity < 8; ++entity)
        {
            if (ecs.IsAlive(entity))
                return false;
        }
        return true;
    }

    // text with its first line ("Version: ...") replaced by line, or removed when line is empty.
    std::string WithVersionLine(const std::string& text, const std::string& line)
    {
        REQUIRE(text.starts_with("Version: "));
        const std::string rest = text.substr(text.find('\n') + 1);
        return line.empty() ? rest : line + "\n" + rest;
    }
}

TEST_CASE("EcsSerializer - a document starts with the format version")
{
    EcsSerialization::ComponentSerializerRegistry registry;
    ECS::ECS ecs = MakeEcs();
    BuildScene(ecs);

    const std::string text = EcsSerialization::EcsSerializer::SerializeToString(registry, ecs, "Versioned");
    CHECK(text.starts_with("Version: " + std::to_string(EcsSerialization::EcsSerializer::BASE_FORMAT_VERSION) +
                           "\nScene name: Versioned\n"));
    CHECK(EcsSerialization::EcsSerializer::SerializeToNode(registry, ecs, "Versioned")["Version"].as<int>() ==
          EcsSerialization::EcsSerializer::BASE_FORMAT_VERSION);
}

TEST_CASE("EcsSerializer - a node round trip equals the string and file round trips")
{
    TempDir tmp;
    FS::FileSystemManager fileSystem;
    MountScene(fileSystem, tmp.Path());

    EcsSerialization::ComponentSerializerRegistry registry;
    registry.RegisterVisitable<TestTransform>("TestTransform");

    ECS::ECS source = MakeEcs();
    BuildScene(source);
    const std::string text = EcsSerialization::EcsSerializer::SerializeToString(registry, source, "Nodes");

    ECS::ECS    fromNode = MakeEcs();
    std::string nodeName;
    REQUIRE(EcsSerialization::EcsSerializer::DeserializeFromNode(
        registry, fromNode, nodeName, EcsSerialization::EcsSerializer::SerializeToNode(registry, source, "Nodes"), "node"));
    CHECK(nodeName == "Nodes");
    CHECK(fromNode.GetRoot() == source.GetRoot());

    REQUIRE(EcsSerialization::EcsSerializer::Serialize(registry, source, "Nodes", "scene://nodes.yaml", fileSystem));
    ECS::ECS    fromFile = MakeEcs();
    std::string fileName;
    REQUIRE(EcsSerialization::EcsSerializer::Deserialize(registry, fromFile, fileName, "scene://nodes.yaml", fileSystem));

    CHECK(EcsSerialization::EcsSerializer::SerializeToString(registry, fromNode, nodeName) == text);
    CHECK(EcsSerialization::EcsSerializer::SerializeToString(registry, fromFile, fileName) == text);
}

TEST_CASE("EcsSerializer - a document without a Version is version 1 and loads unchanged")
{
    EcsSerialization::ComponentSerializerRegistry registry;
    registry.RegisterVisitable<TestTransform>("TestTransform");

    ECS::ECS source = MakeEcs();
    BuildScene(source);
    const std::string text = EcsSerialization::EcsSerializer::SerializeToString(registry, source, "Legacy");

    ECS::ECS    target = MakeEcs();
    std::string sceneName;
    REQUIRE(EcsSerialization::EcsSerializer::DeserializeFromString(
        registry, target, sceneName, WithVersionLine(text, ""), "legacy"));
    CHECK(sceneName == "Legacy");
    CHECK(EcsSerialization::EcsSerializer::SerializeToString(registry, target, sceneName) == text); // saving writes Version
}

TEST_CASE("EcsSerializer - a newer or unreadable version is refused without touching the ECS")
{
    EcsSerialization::ComponentSerializerRegistry registry;
    registry.RegisterVisitable<TestTransform>("TestTransform");

    ECS::ECS source = MakeEcs();
    BuildScene(source);
    const std::string text = EcsSerialization::EcsSerializer::SerializeToString(registry, source, "Future");
    const int         newer = EcsSerialization::EcsSerializer::FORMAT_VERSION + 1;

    {
        LogCapture  log;
        ECS::ECS    target = MakeEcs();
        std::string sceneName = "unchanged";
        CHECK_FALSE(EcsSerialization::EcsSerializer::DeserializeFromString(
            registry, target, sceneName, WithVersionLine(text, "Version: " + std::to_string(newer)), "future.yaml"));
        CHECK(IsEmpty(target));
        CHECK(sceneName == "unchanged");
        CHECK(log.Lines("future.yaml is format version " + std::to_string(newer) +
                        ", but this build reads up to version " +
                        std::to_string(EcsSerialization::EcsSerializer::FORMAT_VERSION)).size() == 1);
    }

    for (const char* bad : { "Version: 0", "Version: -3", "Version: two", "Version: 1.5", "Version: [1]" })
    {
        CAPTURE(bad);
        LogCapture  log;
        ECS::ECS    target = MakeEcs();
        std::string sceneName;
        CHECK_FALSE(EcsSerialization::EcsSerializer::DeserializeFromString(
            registry, target, sceneName, WithVersionLine(text, bad), "bad.yaml"));
        CHECK(IsEmpty(target));
        CHECK(log.Lines("bad.yaml has a Version that is not a positive integer").size() == 1);
    }
}

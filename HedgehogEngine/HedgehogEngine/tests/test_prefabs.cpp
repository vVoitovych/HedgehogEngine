#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/ECS/components/PrefabInstanceComponent.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/Prefab/PrefabManager.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "EcsSerialization/api/EcsSerializer.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include "Logger/api/Logger.hpp"

#include "yaml-cpp/yaml.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

using HedgehogEngine::EngineContext;
using HedgehogEngine::PrefabInstanceComponent;
using HedgehogEngine::TransformComponent;

namespace
{
    // Every Logger line while it lives, through a sink (the engine's tests cannot use
    // HedgehogScripting's console capture, which sits above them).
    class LogCapture
    {
    public:
        LogCapture()
            : m_Sink(EngineLogger::Logger::Instance().AddSink([this](EngineLogger::LogLevel, const std::string& line)
                                                               { m_Text += line + '\n'; }))
        {
        }
        ~LogCapture() { EngineLogger::Logger::Instance().RemoveSink(m_Sink); }

        LogCapture(const LogCapture&)            = delete;
        LogCapture& operator=(const LogCapture&) = delete;

        const std::string& Text() const { return m_Text; }

        size_t Count(const std::string& fragment) const
        {
            size_t count = 0;
            for (size_t at = m_Text.find(fragment); at != std::string::npos; at = m_Text.find(fragment, at + 1))
                ++count;
            return count;
        }

    private:
        std::string m_Text;
        int         m_Sink;
    };

    // An engine with prefabs:// on a temp folder and a lamp (with a bulb and a shade) to make a
    // prefab of, beside a table outside it.
    struct PrefabWorld
    {
        TempDir       Dir;
        EngineContext Context;
        ECS::ECS&     Ecs    = Context.GetECS();
        ECS::Entity   Table  = Context.GetSceneManager().CreateGameObject();
        ECS::Entity   Lamp   = Context.GetSceneManager().CreateGameObject();
        ECS::Entity   Bulb   = Context.GetSceneManager().CreateGameObject(Lamp);
        ECS::Entity   Shade  = Context.GetSceneManager().CreateGameObject(Lamp);

        PrefabWorld()
        {
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("prefabs://", Dir.Path());
            REQUIRE(Context.GetFileSystem().Register(std::move(fs)));

            Ecs.GetComponent<TransformComponent>(Lamp).Position = HM::Vector3(1.0f, 2.0f, 3.0f);
            Ecs.GetComponent<TransformComponent>(Shade).Scale   = HM::Vector3(0.5f, 0.5f, 0.5f);
            HedgehogEngine::LightComponent light;
            light.Intensity = 2.5f;
            Ecs.AddComponent(Bulb, light);

            // The shade's script names the bulb (inside the prefab) and the table (outside it).
            HedgehogEngine::ScriptComponent script;
            script.ScriptPath = "assets://Scripts/Shade.lua";
            HedgehogEngine::SetScriptProperty(script, { "bulb", HedgehogEngine::ScriptPropertyType::EntityRef, Bulb, {} });
            HedgehogEngine::SetScriptProperty(script, { "table", HedgehogEngine::ScriptPropertyType::EntityRef, Table, {} });
            Ecs.AddComponent(Shade, script);
        }

        HedgehogEngine::PrefabManager& Prefabs() { return Context.GetPrefabs(); }

        const ECS::HierarchyComponent& Hierarchy(ECS::Entity entity) { return Ecs.GetComponent<ECS::HierarchyComponent>(entity); }

        // The subtree's entities as YAML without their prefab links or script properties (whose
        // entity ids differ by design), for comparing a source with an instance.
        std::string Describe(ECS::Entity entity)
        {
            YAML::Node subtree =
                EcsSerialization::EcsSerializer::SerializeSubtree(Context.GetComponentRegistry(), Ecs, entity)["Subtree"];
            std::vector<YAML::Node> pending{ subtree[0] };
            while (!pending.empty())
            {
                YAML::Node node = pending.back();
                pending.pop_back();
                node.remove("PrefabInstanceComponent");
                if (node["ScriptComponent"])
                    node["ScriptComponent"].remove("ScriptProperties");
                for (YAML::Node child : node["Children"])
                    pending.push_back(child);
            }
            return YAML::Dump(subtree);
        }
    };

    std::string ScriptTarget(ECS::ECS& ecs, ECS::Entity entity, const char* property)
    {
        auto& script = ecs.GetComponent<HedgehogEngine::ScriptComponent>(entity);
        const ECS::Entity target = std::get<ECS::Entity>(HedgehogEngine::FindScriptProperty(script, property)->Value);
        return target == ECS::INVALID_ENTITY ? "none" : std::to_string(target);
    }
}

TEST_CASE("Prefabs - an instance's components equal the source's")
{
    PrefabWorld world;
    REQUIRE(world.Prefabs().CreatePrefab(world.Lamp, "prefabs://Lamp.prefab"));
    CHECK(std::filesystem::exists(world.Dir.Path() / "Lamp.prefab"));

    const ECS::Entity instance = world.Prefabs().Instantiate("prefabs://Lamp.prefab");
    REQUIRE(instance != ECS::INVALID_ENTITY);
    CHECK(world.Hierarchy(instance).Parent == world.Context.GetSceneManager().GetRootEntity());
    CHECK(world.Hierarchy(world.Context.GetSceneManager().GetRootEntity()).Children.back() == instance);
    CHECK(world.Describe(instance) == world.Describe(world.Lamp));
    CHECK(world.Ecs.GetComponent<HedgehogEngine::LightComponent>(world.Hierarchy(instance).Children[0]).Intensity == 2.5f);

    // The bulb reference points at the instance's bulb; the table lies outside the prefab.
    const ECS::Entity shade = world.Hierarchy(instance).Children[1];
    CHECK(ScriptTarget(world.Ecs, shade, "bulb") == std::to_string(world.Hierarchy(instance).Children[0]));
    CHECK(ScriptTarget(world.Ecs, shade, "table") == "none");
    // The source is untouched.
    CHECK(ScriptTarget(world.Ecs, world.Shade, "table") == std::to_string(world.Table));
}

TEST_CASE("Prefabs - instances carry their local ids and root")
{
    PrefabWorld world;
    REQUIRE(world.Prefabs().CreatePrefab(world.Lamp, "prefabs://Lamp.prefab"));
    const ECS::Entity first  = world.Prefabs().Instantiate("prefabs://Lamp.prefab", world.Table);
    const ECS::Entity second = world.Prefabs().Instantiate("prefabs://Lamp.prefab", world.Table);
    REQUIRE(first != ECS::INVALID_ENTITY);
    REQUIRE(second != ECS::INVALID_ENTITY);
    CHECK(first != second);
    CHECK(world.Hierarchy(world.Table).Children == std::vector<ECS::Entity>{ first, second });
    CHECK(world.Prefabs().GetCachedPrefabCount() == 1);

    for (const ECS::Entity instance : { first, second })
    {
        const auto& root = world.Ecs.GetComponent<PrefabInstanceComponent>(instance);
        CHECK(root.PrefabPath == "prefabs://Lamp.prefab");
        CHECK(root.LocalId == 0);
        CHECK(root.InstanceRoot == instance);
        const std::vector<ECS::Entity> children = world.Hierarchy(instance).Children;
        REQUIRE(children.size() == 2);
        for (uint32_t i = 0; i < 2; ++i)
        {
            const auto& link = world.Ecs.GetComponent<PrefabInstanceComponent>(children[i]);
            CHECK(link.PrefabPath.empty());
            CHECK(link.LocalId == i + 1);
            CHECK(link.InstanceRoot == instance);
        }
    }
    // The source has no link.
    CHECK_FALSE(world.Ecs.HasComponent<PrefabInstanceComponent>(world.Lamp));
}

TEST_CASE("Prefabs - a prefab made from an instance links its instances to the new prefab")
{
    PrefabWorld world;
    REQUIRE(world.Prefabs().CreatePrefab(world.Lamp, "prefabs://Lamp.prefab"));
    const ECS::Entity lamp = world.Prefabs().Instantiate("prefabs://Lamp.prefab");
    REQUIRE(world.Prefabs().CreatePrefab(lamp, "prefabs://Lamp2.prefab"));

    const ECS::Entity copy = world.Prefabs().Instantiate("prefabs://Lamp2.prefab");
    REQUIRE(copy != ECS::INVALID_ENTITY);
    CHECK(world.Ecs.GetComponent<PrefabInstanceComponent>(copy).PrefabPath == "prefabs://Lamp2.prefab");
    CHECK(world.Ecs.GetComponent<PrefabInstanceComponent>(world.Hierarchy(copy).Children[1]).InstanceRoot == copy);
}

TEST_CASE("Prefabs - a changed file is read again")
{
    PrefabWorld world;
    REQUIRE(world.Prefabs().CreatePrefab(world.Lamp, "prefabs://Lamp.prefab"));
    REQUIRE(world.Prefabs().Instantiate("prefabs://Lamp.prefab") != ECS::INVALID_ENTITY);

    // CreatePrefab over the same path replaces the cached document.
    world.Ecs.GetComponent<HedgehogEngine::LightComponent>(world.Bulb).Intensity = 0.75f;
    REQUIRE(world.Prefabs().CreatePrefab(world.Lamp, "prefabs://Lamp.prefab"));
    ECS::Entity instance = world.Prefabs().Instantiate("prefabs://Lamp.prefab");
    CHECK(world.Ecs.GetComponent<HedgehogEngine::LightComponent>(world.Hierarchy(instance).Children[0]).Intensity == 0.75f);

    // So does an edit made outside the engine, once the write time moves on.
    const std::filesystem::path file = world.Dir.Path() / "Lamp.prefab";
    std::string                 text = *world.Context.GetFileSystem().ReadTextFile("prefabs://Lamp.prefab");
    const size_t                at   = text.find("LightIntensity: 0.75");
    REQUIRE(at != std::string::npos);
    text.replace(at, 20, "LightIntensity: 1.25");
    const auto writeTime = std::filesystem::last_write_time(file);
    world.Dir.WriteFile("Lamp.prefab", text);
    std::filesystem::last_write_time(file, writeTime + std::chrono::seconds(2));

    instance = world.Prefabs().Instantiate("prefabs://Lamp.prefab");
    CHECK(world.Ecs.GetComponent<HedgehogEngine::LightComponent>(world.Hierarchy(instance).Children[0]).Intensity == 1.25f);
}

TEST_CASE("Prefabs - a missing or broken prefab fails naming its path and creates nothing")
{
    PrefabWorld  world;
    const size_t before = world.Ecs.GetEntityCount();

    {
        LogCapture log;
        CHECK(world.Prefabs().Instantiate("prefabs://Missing.prefab") == ECS::INVALID_ENTITY);
        CHECK(log.Text().find("[Prefab] prefabs://Missing.prefab does not exist.") != std::string::npos);
    }
    {
        world.Dir.WriteFile("Broken.prefab", "Version: 9\nRoot: {}\n");
        LogCapture log;
        CHECK(world.Prefabs().Instantiate("prefabs://Broken.prefab") == ECS::INVALID_ENTITY);
        CHECK(log.Text().find("prefabs://Broken.prefab: the prefab is format version 9") != std::string::npos);
    }
    {
        world.Dir.WriteFile("Empty.prefab", "Version: 1\n");
        LogCapture log;
        CHECK(world.Prefabs().Instantiate("prefabs://Empty.prefab") == ECS::INVALID_ENTITY);
        CHECK(log.Text().find("Root is missing") != std::string::npos);
    }
    {
        LogCapture log;
        CHECK(world.Prefabs().Instantiate("prefabs://Lamp.yaml") == ECS::INVALID_ENTITY);
        CHECK_FALSE(world.Prefabs().CreatePrefab(world.Lamp, "prefabs://Lamp.txt"));
        CHECK_FALSE(world.Prefabs().CreatePrefab(world.Context.GetSceneManager().GetRootEntity(), "prefabs://Root.prefab"));
        CHECK(log.Count("is not a .prefab path") == 2);
        CHECK(log.Text().find("is the scene root") != std::string::npos);
    }
    CHECK(world.Ecs.GetEntityCount() == before);
    CHECK(world.Prefabs().GetCachedPrefabCount() == 0);
}

TEST_CASE("Prefabs - linking the source makes it an instance with the document's local ids")
{
    PrefabWorld world;
    REQUIRE(world.Prefabs().CreatePrefab(world.Lamp, "prefabs://Lamp.prefab"));
    REQUIRE(world.Prefabs().LinkInstance(world.Lamp, "prefabs://Lamp.prefab"));
    const ECS::Entity copy = world.Prefabs().Instantiate("prefabs://Lamp.prefab");
    REQUIRE(copy != ECS::INVALID_ENTITY);

    // The source's links match the instance's node for node.
    const std::vector<ECS::Entity> source{ world.Lamp, world.Bulb, world.Shade };
    const std::vector<ECS::Entity> instance{ copy, world.Hierarchy(copy).Children[0], world.Hierarchy(copy).Children[1] };
    for (size_t i = 0; i < source.size(); ++i)
    {
        CAPTURE(i);
        const auto& link = world.Ecs.GetComponent<PrefabInstanceComponent>(source[i]);
        CHECK(link.LocalId == world.Ecs.GetComponent<PrefabInstanceComponent>(instance[i]).LocalId);
        CHECK(link.InstanceRoot == world.Lamp);
        CHECK(link.PrefabPath == (i == 0 ? "prefabs://Lamp.prefab" : ""));
        CHECK(world.Hierarchy(source[i]).Name == world.Hierarchy(instance[i]).Name);
    }

    LogCapture log;
    CHECK_FALSE(world.Prefabs().LinkInstance(world.Lamp, "prefabs://Lamp.yaml"));
    CHECK_FALSE(world.Prefabs().LinkInstance(world.Context.GetSceneManager().GetRootEntity(), "prefabs://Lamp.prefab"));
    CHECK(log.Count("[Prefab]") == 2);
}

TEST_CASE("Prefabs - the shipped LampPost prefab instantiates with its mesh and light")
{
    EngineContext context;
    ECS::ECS&     ecs = context.GetECS();

    const ECS::Entity lampPost = context.GetPrefabs().Instantiate("assets://Prefabs/LampPost.prefab");
    REQUIRE(lampPost != ECS::INVALID_ENTITY);
    const auto& children = ecs.GetComponent<ECS::HierarchyComponent>(lampPost).Children;
    REQUIRE(children.size() == 2);
    CHECK(ecs.GetComponent<ECS::HierarchyComponent>(children[0]).Name == "Post");
    const ECS::Entity lamp = children[1];
    REQUIRE(ecs.HasComponent<HedgehogEngine::LightComponent>(lamp));
    CHECK(ecs.GetComponent<HedgehogEngine::LightComponent>(lamp).LightType == HedgehogEngine::LightType::PointLight);
    CHECK(ecs.GetComponent<PrefabInstanceComponent>(lamp).LocalId == 2);
    CHECK(ecs.GetComponent<PrefabInstanceComponent>(lampPost).PrefabPath == "assets://Prefabs/LampPost.prefab");
}

TEST_CASE("Prefabs - scenes keep instances as references with overrides")
{
    PrefabWorld world;
    auto&       scenes = world.Context.GetSceneManager();
    REQUIRE(world.Prefabs().CreatePrefab(world.Lamp, "prefabs://Lamp.prefab"));
    const ECS::Entity plain  = world.Prefabs().Instantiate("prefabs://Lamp.prefab");
    const ECS::Entity bright = world.Prefabs().Instantiate("prefabs://Lamp.prefab");
    const ECS::Entity brightBulb = world.Hierarchy(bright).Children[0];
    world.Ecs.GetComponent<HedgehogEngine::LightComponent>(brightBulb).Intensity = 3.0f;

    const std::string scenePath = (world.Dir.Path() / "Street.yaml").string();
    REQUIRE(scenes.SaveScene(scenePath));
    const std::string text = *world.Context.GetFileSystem().ReadTextFile("prefabs://Street.yaml");
    CHECK(text.starts_with("Version: 2\n"));
    CHECK(text.find("Prefab: prefabs://Lamp.prefab") != std::string::npos);

    // Load it back: both instances as they were, the scene re-saving to the same text.
    REQUIRE(scenes.LoadScene(scenePath));
    CHECK(world.Ecs.GetComponent<HedgehogEngine::LightComponent>(world.Hierarchy(plain).Children[0]).Intensity == 2.5f);
    CHECK(world.Ecs.GetComponent<HedgehogEngine::LightComponent>(brightBulb).Intensity == 3.0f);
    CHECK(world.Ecs.GetComponent<PrefabInstanceComponent>(brightBulb).InstanceRoot == bright);
    CHECK(scenes.CaptureSnapshot().Yaml == text);

    // Change the prefab on disk: the plain instance follows, the overridden bulb keeps its intensity.
    std::string prefabText = *world.Context.GetFileSystem().ReadTextFile("prefabs://Lamp.prefab");
    const size_t at = prefabText.find("LightIntensity: 2.5");
    REQUIRE(at != std::string::npos);
    prefabText.replace(at, 19, "LightIntensity: 0.5");
    const std::filesystem::path prefabFile = world.Dir.Path() / "Lamp.prefab";
    const auto                  writeTime  = std::filesystem::last_write_time(prefabFile);
    world.Dir.WriteFile("Lamp.prefab", prefabText);
    std::filesystem::last_write_time(prefabFile, writeTime + std::chrono::seconds(2));

    REQUIRE(scenes.LoadScene(scenePath));
    CHECK(world.Ecs.GetComponent<HedgehogEngine::LightComponent>(world.Hierarchy(plain).Children[0]).Intensity == 0.5f);
    CHECK(world.Ecs.GetComponent<HedgehogEngine::LightComponent>(brightBulb).Intensity == 3.0f);
}

TEST_CASE("Prefabs - Play and Stop bring an edited instance back")
{
    PrefabWorld world;
    REQUIRE(world.Prefabs().CreatePrefab(world.Lamp, "prefabs://Lamp.prefab"));
    const ECS::Entity lamp = world.Prefabs().Instantiate("prefabs://Lamp.prefab");
    const ECS::Entity bulb = world.Hierarchy(lamp).Children[0];
    world.Ecs.GetComponent<HedgehogEngine::LightComponent>(bulb).Intensity = 1.75f;

    REQUIRE(world.Context.Play());
    world.Ecs.GetComponent<HedgehogEngine::LightComponent>(bulb).Intensity = 0.0f;
    world.Context.GetSceneManager().DeleteGameObject(lamp);
    REQUIRE(world.Context.Stop());

    REQUIRE(world.Ecs.IsAlive(bulb));
    CHECK(world.Ecs.GetComponent<HedgehogEngine::LightComponent>(bulb).Intensity == 1.75f);
    CHECK(world.Ecs.GetComponent<PrefabInstanceComponent>(bulb).InstanceRoot == lamp);
}

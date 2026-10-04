#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/ECS/components/PrefabInstanceComponent.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/Prefab/PrefabManager.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "EcsSerialization/api/ComponentSerializerRegistry.hpp"
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
            // Every entity in full, an instance included, as a registry without the prefab provider writes it.
            EcsSerialization::ComponentSerializerRegistry registry = Context.GetComponentRegistry();
            registry.SetPrefabProvider(nullptr);
            YAML::Node subtree = EcsSerialization::EcsSerializer::SerializeSubtree(registry, Ecs, entity)["Subtree"];
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

namespace
{
    // The overrides of one component on one node.
    EcsSerialization::OverrideSet Only(const EcsSerialization::OverrideSet& overrides, uint32_t localId, const std::string& component)
    {
        EcsSerialization::OverrideSet found;
        for (const auto& entry : overrides)
        {
            if (entry.LocalId == localId && entry.Component == component)
                found.push_back(entry);
        }
        return found;
    }
}

TEST_CASE("Prefab overrides - a fresh instance overrides nothing, an edit is one override that Revert clears")
{
    PrefabWorld world;
    REQUIRE(world.Prefabs().CreatePrefab(world.Lamp, "prefabs://Lamp.prefab"));
    const ECS::Entity lamp = world.Prefabs().Instantiate("prefabs://Lamp.prefab");
    const ECS::Entity bulb = world.Hierarchy(lamp).Children[0];
    // The shade's script names the bulb inside the prefab: its copy's id is not an override.
    CHECK(world.Prefabs().GetOverrides(bulb).empty());
    CHECK(world.Prefabs().GetInstanceRoot(bulb) == lamp);
    CHECK(world.Prefabs().GetInstanceRoot(world.Lamp) == ECS::INVALID_ENTITY);

    world.Ecs.GetComponent<HedgehogEngine::LightComponent>(bulb).Intensity = 0.25f;
    const EcsSerialization::OverrideSet overrides = world.Prefabs().GetOverrides(bulb);
    REQUIRE(overrides.size() == 1);
    CHECK(overrides[0].LocalId == 1);
    CHECK(overrides[0].Component == "LightComponent");
    CHECK(overrides[0].Property == "LightIntensity");

    REQUIRE(world.Prefabs().Revert(bulb, overrides));
    CHECK(world.Ecs.GetComponent<HedgehogEngine::LightComponent>(bulb).Intensity == 2.5f);
    CHECK(world.Prefabs().GetOverrides(lamp).empty());

    // A component added on the instance is a whole-component override; reverting removes it.
    world.Ecs.AddComponent(lamp, HedgehogEngine::LightComponent{});
    const EcsSerialization::OverrideSet added = Only(world.Prefabs().GetOverrides(lamp), 0, "LightComponent");
    REQUIRE(added.size() == 1);
    CHECK(added[0].Property.empty());
    REQUIRE(world.Prefabs().Revert(lamp, added));
    CHECK_FALSE(world.Ecs.HasComponent<HedgehogEngine::LightComponent>(lamp));
}

TEST_CASE("Prefab overrides - Apply updates the file and every other instance, which keep their own overrides")
{
    PrefabWorld world;
    REQUIRE(world.Prefabs().CreatePrefab(world.Lamp, "prefabs://Lamp.prefab"));
    const ECS::Entity editing = world.Prefabs().Instantiate("prefabs://Lamp.prefab");
    const ECS::Entity plain   = world.Prefabs().Instantiate("prefabs://Lamp.prefab");
    const ECS::Entity dimmed  = world.Prefabs().Instantiate("prefabs://Lamp.prefab");
    auto              light   = [&world](ECS::Entity lamp) -> HedgehogEngine::LightComponent&
    { return world.Ecs.GetComponent<HedgehogEngine::LightComponent>(world.Hierarchy(lamp).Children[0]); };
    light(dimmed).Intensity = 0.1f;

    // Applied from the bulb, a node below the instance's root.
    light(editing).Intensity = 1.5f;
    world.Ecs.GetComponent<TransformComponent>(world.Hierarchy(editing).Children[1]).Scale = HM::Vector3(3.0f, 3.0f, 3.0f);
    REQUIRE(world.Prefabs().Apply(world.Hierarchy(editing).Children[0], world.Prefabs().GetOverrides(editing)));

    CHECK(world.Prefabs().GetOverrides(editing).empty());
    CHECK(light(plain).Intensity == 1.5f);
    CHECK(world.Ecs.GetComponent<TransformComponent>(world.Hierarchy(plain).Children[1]).Scale == HM::Vector3(3.0f, 3.0f, 3.0f));
    CHECK(light(dimmed).Intensity == 0.1f); // its own override survives
    CHECK(world.Ecs.GetComponent<TransformComponent>(world.Hierarchy(dimmed).Children[1]).Scale == HM::Vector3(3.0f, 3.0f, 3.0f));
    CHECK(world.Prefabs().GetOverrides(plain).empty());
    CHECK(Only(world.Prefabs().GetOverrides(dimmed), 1, "LightComponent").size() == 1);

    // The file holds the applied values: a new instance has them.
    const ECS::Entity fresh = world.Prefabs().Instantiate("prefabs://Lamp.prefab");
    CHECK(light(fresh).Intensity == 1.5f);

    // A component added on one instance and applied reaches the others.
    world.Ecs.AddComponent(editing, HedgehogEngine::LightComponent{});
    REQUIRE(world.Prefabs().Apply(editing, Only(world.Prefabs().GetOverrides(editing), 0, "LightComponent")));
    CHECK(world.Ecs.HasComponent<HedgehogEngine::LightComponent>(plain));
    CHECK(world.Prefabs().GetOverrides(plain).empty());

    LogCapture log;
    CHECK_FALSE(world.Prefabs().Apply(world.Lamp, {}));
    CHECK_FALSE(world.Prefabs().Revert(world.Lamp, {}));
    CHECK(log.Count("is not part of a prefab instance") == 2);
}

namespace
{
    // Lamp.prefab (Lamp > Bulb, Shade) and Street.prefab (Street > a Lamp instance whose bulb is
    // overridden to 1.0), built through the editor's flow.
    struct StreetWorld : PrefabWorld
    {
        StreetWorld()
        {
            REQUIRE(Prefabs().CreatePrefab(Lamp, "prefabs://Lamp.prefab"));
            const ECS::Entity street = Context.GetSceneManager().CreateGameObject();
            Ecs.GetComponent<ECS::HierarchyComponent>(street).Name = "Street";
            const ECS::Entity lamp = Prefabs().Instantiate("prefabs://Lamp.prefab", street);
            Ecs.GetComponent<HedgehogEngine::LightComponent>(Hierarchy(lamp).Children[0]).Intensity = 1.0f;
            REQUIRE(Prefabs().CreatePrefab(street, "prefabs://Street.prefab"));
        }

        // A street instance's nested lamp, bulb and shade.
        ECS::Entity NestedLamp(ECS::Entity street) { return Hierarchy(street).Children[0]; }
        ECS::Entity NestedBulb(ECS::Entity street) { return Hierarchy(NestedLamp(street)).Children[0]; }
        ECS::Entity NestedShade(ECS::Entity street) { return Hierarchy(NestedLamp(street)).Children[1]; }
        float       BulbIntensity(ECS::Entity street) { return Ecs.GetComponent<HedgehogEngine::LightComponent>(NestedBulb(street)).Intensity; }

        void RewriteLamp(const std::string& from, const std::string& to)
        {
            std::string text = *Context.GetFileSystem().ReadTextFile("prefabs://Lamp.prefab");
            const size_t at  = text.find(from);
            REQUIRE(at != std::string::npos);
            text.replace(at, from.size(), to);
            const std::filesystem::path file      = Dir.Path() / "Lamp.prefab";
            const auto                  writeTime = std::filesystem::last_write_time(file);
            Dir.WriteFile("Lamp.prefab", text);
            std::filesystem::last_write_time(file, writeTime + std::chrono::seconds(2));
        }
    };
}

TEST_CASE("Nested prefabs - an outer prefab keeps its nested instance as a reference with overrides")
{
    StreetWorld world;
    const std::string text = *world.Context.GetFileSystem().ReadTextFile("prefabs://Street.prefab");
    const YAML::Node  lamp = YAML::Load(text)["Root"]["Subtree"][0]["Children"][0];
    CHECK(lamp["Prefab"].as<std::string>() == "prefabs://Lamp.prefab");
    CHECK(lamp["Entities"].as<std::vector<ECS::Entity>>() == std::vector<ECS::Entity>{ 1, 2, 3 });
    REQUIRE(lamp["Overrides"].size() == 1);
    CHECK(lamp["Overrides"][0]["LocalId"].as<int>() == 1);
    CHECK(lamp["Overrides"][0]["Value"].as<float>() == 1.0f);
    CHECK(text.find("Bulb") == std::string::npos); // the lamp's own entities are not copied

    // Instantiated, the street holds the whole lamp, one instance numbered in the street's ids.
    const ECS::Entity street = world.Prefabs().Instantiate("prefabs://Street.prefab");
    REQUIRE(street != ECS::INVALID_ENTITY);
    CHECK(world.Hierarchy(world.NestedLamp(street)).Name == world.Hierarchy(world.Lamp).Name);
    CHECK(world.BulbIntensity(street) == 1.0f);
    CHECK(world.Ecs.GetComponent<PrefabInstanceComponent>(world.NestedShade(street)).LocalId == 3);
    CHECK(world.Ecs.GetComponent<PrefabInstanceComponent>(world.NestedShade(street)).InstanceRoot == street);
    CHECK(world.Prefabs().GetOverrides(street).empty());
    // The shade's script still names its own bulb.
    CHECK(ScriptTarget(world.Ecs, world.NestedShade(street), "bulb") == std::to_string(world.NestedBulb(street)));
}

TEST_CASE("Nested prefabs - editing the inner prefab reaches scene instances of the outer one")
{
    StreetWorld world;
    auto&       scenes = world.Context.GetSceneManager();
    const ECS::Entity street = world.Prefabs().Instantiate("prefabs://Street.prefab");
    const std::string scenePath = (world.Dir.Path() / "Town.yaml").string();
    REQUIRE(scenes.SaveScene(scenePath));

    world.RewriteLamp("Scale: [0.5, 0.5, 0.5]", "Scale: [4, 4, 4]");
    world.RewriteLamp("LightIntensity: 2.5", "LightIntensity: 9");
    REQUIRE(scenes.LoadScene(scenePath));
    CHECK(world.Ecs.GetComponent<TransformComponent>(world.NestedShade(street)).Scale == HM::Vector3(4.0f, 4.0f, 4.0f));
    CHECK(world.BulbIntensity(street) == 1.0f); // the street's own override of the lamp wins
}

TEST_CASE("Nested prefabs - an override on a nested entity round-trips through a scene")
{
    StreetWorld world;
    auto&       scenes = world.Context.GetSceneManager();
    const ECS::Entity street = world.Prefabs().Instantiate("prefabs://Street.prefab");
    world.Ecs.GetComponent<HedgehogEngine::LightComponent>(world.NestedBulb(street)).Intensity = 0.3f;
    const EcsSerialization::OverrideSet overrides = world.Prefabs().GetOverrides(street);
    REQUIRE(overrides.size() == 1);
    CHECK(overrides[0].LocalId == 2);

    const std::string scenePath = (world.Dir.Path() / "Town.yaml").string();
    REQUIRE(scenes.SaveScene(scenePath));
    const std::string text = *world.Context.GetFileSystem().ReadTextFile("prefabs://Town.yaml");
    REQUIRE(scenes.LoadScene(scenePath));
    CHECK(world.BulbIntensity(street) == 0.3f);
    CHECK(scenes.CaptureSnapshot().Yaml == text);
}

TEST_CASE("Nested prefabs - Apply from a nested entity overrides the nested instance in the outer prefab")
{
    StreetWorld world;
    const ECS::Entity first  = world.Prefabs().Instantiate("prefabs://Street.prefab");
    const ECS::Entity second = world.Prefabs().Instantiate("prefabs://Street.prefab");
    world.Ecs.GetComponent<HedgehogEngine::LightComponent>(world.NestedBulb(first)).Intensity = 0.7f;
    REQUIRE(world.Prefabs().Apply(world.NestedBulb(first), world.Prefabs().GetOverrides(first)));

    CHECK(world.BulbIntensity(second) == 0.7f);
    CHECK(world.Prefabs().GetOverrides(first).empty());
    const YAML::Node lamp =
        YAML::Load(*world.Context.GetFileSystem().ReadTextFile("prefabs://Street.prefab"))["Root"]["Subtree"][0]["Children"][0];
    REQUIRE(lamp["Overrides"].size() == 1); // replaced, not added beside the old one
    CHECK(lamp["Overrides"][0]["Value"].as<float>() == 0.7f);
    // The lamp prefab itself is untouched.
    CHECK(world.Context.GetFileSystem().ReadTextFile("prefabs://Lamp.prefab")->find("LightIntensity: 2.5") != std::string::npos);
}

TEST_CASE("Nested prefabs - a prefab that would contain itself is refused at create and at load")
{
    StreetWorld world;
    // A Town holding a Street, saved as Lamp.prefab, would make Lamp -> Street -> Lamp.
    const ECS::Entity town = world.Context.GetSceneManager().CreateGameObject();
    REQUIRE(world.Prefabs().Instantiate("prefabs://Street.prefab", town) != ECS::INVALID_ENTITY);
    {
        LogCapture log;
        CHECK_FALSE(world.Prefabs().CreatePrefab(town, "prefabs://Lamp.prefab"));
        CHECK(log.Count("prefabs://Lamp.prefab would contain itself: prefabs://Lamp.prefab -> prefabs://Street.prefab -> "
                        "prefabs://Lamp.prefab") == 1);
    }
    // Written by hand anyway, the cycle is found when it is read.
    REQUIRE(world.Prefabs().CreatePrefab(town, "prefabs://Town.prefab"));
    const std::string           townText  = *world.Context.GetFileSystem().ReadTextFile("prefabs://Town.prefab");
    const std::filesystem::path lampFile  = world.Dir.Path() / "Lamp.prefab";
    const auto                  writeTime = std::filesystem::last_write_time(lampFile);
    world.Dir.WriteFile("Lamp.prefab", townText);
    std::filesystem::last_write_time(lampFile, writeTime + std::chrono::seconds(2));

    LogCapture   log;
    const size_t before = world.Ecs.GetEntityCount();
    CHECK(world.Prefabs().Instantiate("prefabs://Street.prefab") == ECS::INVALID_ENTITY);
    CHECK(log.Count("prefab cycle: prefabs://Street.prefab -> prefabs://Lamp.prefab -> prefabs://Street.prefab") == 1);
    CHECK(world.Ecs.GetEntityCount() == before);
}

TEST_CASE("Nested prefabs - a node the inner prefab gained appears in the outer one")
{
    StreetWorld world;
    // The lamp gains a third child, written as the editor would.
    const ECS::Entity cap = world.Context.GetSceneManager().CreateGameObject(world.Lamp);
    world.Ecs.GetComponent<ECS::HierarchyComponent>(cap).Name = "Cap";
    REQUIRE(world.Prefabs().CreatePrefab(world.Lamp, "prefabs://Lamp.prefab"));

    const ECS::Entity street = world.Prefabs().Instantiate("prefabs://Street.prefab");
    REQUIRE(street != ECS::INVALID_ENTITY);
    const auto& children = world.Hierarchy(world.NestedLamp(street)).Children;
    REQUIRE(children.size() == 3);
    CHECK(world.Hierarchy(children[2]).Name == "Cap");
    CHECK(world.Ecs.GetComponent<PrefabInstanceComponent>(children[2]).LocalId == 4); // past the street's own ids
    CHECK(world.BulbIntensity(street) == 1.0f);
}

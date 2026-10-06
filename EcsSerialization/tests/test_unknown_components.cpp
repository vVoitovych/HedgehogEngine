#include "doctest/doctest/doctest.h"

#include "EcsSerialization/api/ComponentSerializerRegistry.hpp"
#include "EcsSerialization/api/EcsSerializer.hpp"
#include "EcsSerialization/api/UnknownComponents.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include "yaml-cpp/yaml.h"

#include <string>
#include <vector>

using EcsSerialization::EcsSerializer;
using EcsSerialization::UnknownComponent;
using EcsSerialization::UnknownComponentsComponent;

namespace
{
    // A component with a handler, so the tests see known and unknown components side by side.
    struct Known
    {
        float Value = 0.0f;

        template<typename V>
        void Visit(V& v)
        {
            v("Value", Value);
        }
    };

    struct World
    {
        ECS::ECS                                      Ecs;
        EcsSerialization::ComponentSerializerRegistry Registry;

        explicit World(bool keepUnknown = true)
        {
            Ecs.Init();
            Ecs.RegisterComponent<ECS::HierarchyComponent>();
            Ecs.RegisterComponent<Known>();
            if (keepUnknown)
                Ecs.RegisterComponent<UnknownComponentsComponent>();
            Registry.RegisterVisitable<Known>("Known");
        }

        bool Load(const std::string& text)
        {
            std::string name;
            return EcsSerializer::DeserializeFromString(Registry, Ecs, name, text, "test scene");
        }

        std::string Save() const { return EcsSerializer::SerializeToString(Registry, Ecs, "Test"); }

        const std::vector<UnknownComponent>& Unknown(ECS::Entity entity)
        {
            REQUIRE(Ecs.HasComponent<UnknownComponentsComponent>(entity));
            return Ecs.GetComponent<UnknownComponentsComponent>(entity).Entries;
        }
    };

    // The root (entity 0) holding rootComponents, with one child (entity 1) holding childComponents.
    std::string MakeScene(const std::string& rootComponents, const std::string& childComponents = {})
    {
        return "Version: 1\nScene name: Test\nScene:\n"
               "  - Entity: 0\n    Name: Root\n    Parent: 4294967295\n" +
               rootComponents +
               "    Children:\n"
               "      - Entity: 1\n        Name: Child\n        Parent: 0\n" +
               childComponents + "        Children: []\n";
    }

    // The node an entity of a saved scene has under key.
    YAML::Node ComponentOf(const std::string& scene, size_t depth, const std::string& key)
    {
        YAML::Node entity = YAML::Load(scene)["Scene"][0];
        for (size_t i = 0; i < depth; ++i)
            entity = entity["Children"][0];
        return entity[key];
    }
}

TEST_CASE("Unknown components - a component no handler reads is kept and written back")
{
    World      world;
    LogCapture log;
    REQUIRE(world.Load(MakeScene("    SpinnerComponent: { Speed: 90 }\n    Known: { Value: 2 }\n")));

    CHECK(log.Lines("component 'SpinnerComponent' has no registered type").size() == 1);
    CHECK(log.Lines("test scene").size() == 1);
    CHECK(world.Ecs.GetComponent<Known>(0).Value == 2.0f);
    REQUIRE(world.Unknown(0).size() == 1);
    CHECK(world.Unknown(0)[0].Key == "SpinnerComponent");

    const std::string saved = world.Save();
    const YAML::Node  spinner = ComponentOf(saved, 0, "SpinnerComponent");
    REQUIRE(spinner.IsMap());
    CHECK(spinner["Speed"].as<int>() == 90);
    CHECK(ComponentOf(saved, 0, "Known")["Value"].as<float>() == 2.0f);

    // Loaded again, it is written the same way.
    World again;
    REQUIRE(again.Load(saved));
    CHECK(again.Save() == saved);
}

TEST_CASE("Unknown components - several keys keep their order and values, one warning per key")
{
    World      world;
    LogCapture log;
    REQUIRE(world.Load(MakeScene("    Alpha: { A: 1 }\n    Beta: [1, 2, 3]\n",
                                 "        Alpha: { A: 5 }\n        Gamma: text\n")));

    CHECK(log.Lines("component 'Alpha'").size() == 1);
    CHECK(log.Lines("component 'Beta'").size() == 1);
    CHECK(log.Lines("component 'Gamma'").size() == 1);

    REQUIRE(world.Unknown(0).size() == 2);
    CHECK(world.Unknown(0)[0].Key == "Alpha");
    CHECK(world.Unknown(0)[1].Key == "Beta");
    REQUIRE(world.Unknown(1).size() == 2);
    CHECK(world.Unknown(1)[0].Key == "Alpha");
    CHECK(world.Unknown(1)[1].Key == "Gamma");

    const std::string saved = world.Save();
    CHECK(ComponentOf(saved, 0, "Alpha")["A"].as<int>() == 1);
    CHECK(ComponentOf(saved, 0, "Beta").size() == 3);
    CHECK(ComponentOf(saved, 1, "Alpha")["A"].as<int>() == 5);
    CHECK(ComponentOf(saved, 1, "Gamma").as<std::string>() == "text");
    // Written after the known components, in the order read.
    CHECK(saved.find("Alpha") < saved.find("Beta"));
}

TEST_CASE("Unknown components - an unknown component in a subtree is kept on the instantiated entity")
{
    World world;
    REQUIRE(world.Load(MakeScene("", "        SpinnerComponent: { Speed: 45 }\n")));
    const YAML::Node subtree = EcsSerializer::SerializeSubtree(world.Registry, world.Ecs, 1);

    LogCapture        log;
    const ECS::Entity copy = EcsSerializer::InstantiateSubtree(world.Registry, world.Ecs, subtree, 0, "test prefab");
    REQUIRE(copy != ECS::INVALID_ENTITY);
    REQUIRE(world.Unknown(copy).size() == 1);
    CHECK(world.Unknown(copy)[0].Key == "SpinnerComponent");
    CHECK(YAML::Load(world.Unknown(copy)[0].Yaml)["Speed"].as<int>() == 45);
    CHECK(log.Lines("test prefab: component 'SpinnerComponent'").size() == 1);
}

TEST_CASE("Unknown components - without the store type the data is dropped with a warning")
{
    World      world(false);
    LogCapture log;
    REQUIRE(world.Load(MakeScene("    SpinnerComponent: { Speed: 90 }\n")));

    CHECK(log.Lines("component 'SpinnerComponent' has no registered type (is its plugin loaded?); its data is dropped.").size() == 1);
    CHECK_FALSE(YAML::Load(world.Save())["Scene"][0]["SpinnerComponent"]);
}

TEST_CASE("Unknown components - the entity keys of a node are reserved")
{
    for (const char* key : { "Entity", "Name", "Parent", "Children", "Prefab", "Entities", "Overrides", "PrefabOrigin" })
        CHECK(EcsSerialization::IsReservedEntityKey(key));
    CHECK_FALSE(EcsSerialization::IsReservedEntityKey("SpinnerComponent"));
    CHECK_FALSE(EcsSerialization::IsReservedEntityKey("name"));
}

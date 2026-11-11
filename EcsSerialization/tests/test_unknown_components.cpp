#include "doctest/doctest/doctest.h"

#include "EcsSerialization/api/ComponentSerializerRegistry.hpp"
#include "EcsSerialization/api/EcsSerializer.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include "yaml-cpp/yaml.h"

#include <string>

using EcsSerialization::EcsSerializer;

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

        World()
        {
            Ecs.Init();
            Ecs.RegisterComponent<ECS::HierarchyComponent>();
            Ecs.RegisterComponent<Known>();
            Registry.RegisterVisitable<Known>("Known");
        }

        bool Load(const std::string& text)
        {
            std::string name;
            return EcsSerializer::DeserializeFromString(Registry, Ecs, name, text, "test scene");
        }

        std::string Save() const { return EcsSerializer::SerializeToString(Registry, Ecs, "Test"); }
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
}

TEST_CASE("Unknown components - a component no handler reads is dropped with one warning per key")
{
    World      world;
    LogCapture log;
    REQUIRE(world.Load(MakeScene("    Gone: { Speed: 90 }\n    Known: { Value: 2 }\n", "        Gone: { Speed: 45 }\n")));

    CHECK(log.Lines("test scene: component 'Gone' has no registered type; its data is dropped.").size() == 1);
    CHECK(world.Ecs.GetComponent<Known>(0).Value == 2.0f);
    const YAML::Node saved = YAML::Load(world.Save());
    CHECK_FALSE(saved["Scene"][0]["Gone"]);
    CHECK(saved["Scene"][0]["Known"]["Value"].as<float>() == 2.0f);
}

TEST_CASE("Unknown components - the entity keys of a node are reserved")
{
    for (const char* key : { "Entity", "Name", "Parent", "Children", "Prefab", "Entities", "Overrides", "PrefabOrigin" })
        CHECK(EcsSerialization::IsReservedEntityKey(key));
    CHECK_FALSE(EcsSerialization::IsReservedEntityKey("Gone"));
    CHECK_FALSE(EcsSerialization::IsReservedEntityKey("name"));
}

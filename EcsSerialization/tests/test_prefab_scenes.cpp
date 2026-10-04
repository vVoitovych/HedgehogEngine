#include "doctest/doctest/doctest.h"

#include "EcsSerialization/api/ComponentSerializerRegistry.hpp"
#include "EcsSerialization/api/EcsSerializer.hpp"
#include "EcsSerialization/api/Prefab/IPrefabProvider.hpp"
#include "EcsSerialization/api/Prefab/OverrideSet.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include "yaml-cpp/yaml.h"

#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace
{
    struct TestBody
    {
        float       Speed = 1.0f;
        std::string Label;

        static void* SpeedAccessor(void* c) { return &static_cast<TestBody*>(c)->Speed; }
        static void* LabelAccessor(void* c) { return &static_cast<TestBody*>(c)->Label; }

        static std::span<const Reflection::PropertyDescriptor> GetProperties()
        {
            static const Reflection::PropertyDescriptor properties[] = {
                { Reflection::TypeTag::Float, "Speed", SpeedAccessor, Reflection::PropertyFlags::None, 0.0f, 0.0f },
                { Reflection::TypeTag::String, "Label", LabelAccessor, Reflection::PropertyFlags::None, 0.0f, 0.0f },
            };
            return properties;
        }
    };

    // Added on an instance only, to test whole-component overrides.
    struct TestTag
    {
        float Value = 0.0f;

        template<typename Visitor>
        void Visit(Visitor& visitor)
        {
            visitor("Value", Value);
        }
    };

    // The link a provider keeps, standing in for the engine's PrefabInstanceComponent.
    struct TestLink
    {
        std::string PrefabPath;
        uint32_t    LocalId      = 0;
        ECS::Entity InstanceRoot = ECS::INVALID_ENTITY;

        static void* PathAccessor(void* c) { return &static_cast<TestLink*>(c)->PrefabPath; }
        static void* LocalAccessor(void* c) { return &static_cast<TestLink*>(c)->LocalId; }
        static void* RootAccessor(void* c) { return &static_cast<TestLink*>(c)->InstanceRoot; }

        static std::span<const Reflection::PropertyDescriptor> GetProperties()
        {
            static const Reflection::PropertyDescriptor properties[] = {
                { Reflection::TypeTag::String, "PrefabPath", PathAccessor, Reflection::PropertyFlags::None, 0.0f, 0.0f },
                { Reflection::TypeTag::UInt, "LocalId", LocalAccessor, Reflection::PropertyFlags::None, 0.0f, 0.0f },
                { Reflection::TypeTag::Entity, "InstanceRoot", RootAccessor, Reflection::PropertyFlags::EntityRef, 0.0f, 0.0f },
            };
            return properties;
        }
    };

    // Prefabs held in memory by path.
    class TestPrefabs : public EcsSerialization::IPrefabProvider
    {
    public:
        std::map<std::string, std::shared_ptr<const YAML::Node>> Documents;

        std::optional<EcsSerialization::PrefabLink> GetLink(const ECS::ECS& ecs, ECS::Entity entity) const override
        {
            if (!ecs.HasComponent<TestLink>(entity))
                return std::nullopt;
            const auto& link = ecs.GetComponent<TestLink>(entity);
            return EcsSerialization::PrefabLink{ link.PrefabPath, link.LocalId, link.InstanceRoot };
        }

        const char* GetLinkComponentKey() const override { return "TestLink"; }

        std::shared_ptr<const YAML::Node> LoadPrefab(const std::string& path) override
        {
            const auto it = Documents.find(path);
            return it == Documents.end() ? nullptr : it->second;
        }

        void Link(ECS::ECS& ecs, const std::vector<ECS::Entity>& entities, const std::string& path) override
        {
            for (size_t i = 0; i < entities.size(); ++i)
            {
                TestLink link{ i == 0 ? path : std::string{}, static_cast<uint32_t>(i), entities[0] };
                if (ecs.HasComponent<TestLink>(entities[i]))
                    ecs.GetComponent<TestLink>(entities[i]) = link;
                else
                    ecs.AddComponent(entities[i], link);
            }
        }
    };

    struct World
    {
        ECS::ECS    Ecs;
        ECS::Entity Root = ECS::INVALID_ENTITY;

        World()
        {
            Ecs.Init();
            Ecs.RegisterComponent<ECS::HierarchyComponent>();
            Ecs.RegisterComponent<TestBody>();
            Ecs.RegisterComponent<TestTag>();
            Ecs.RegisterComponent<TestLink>();
        }

        ECS::Entity Add(const std::string& name, ECS::Entity parent)
        {
            const ECS::Entity entity = Ecs.CreateEntity();
            Ecs.AddComponent(entity, ECS::HierarchyComponent{ name, parent, {} });
            if (parent != ECS::INVALID_ENTITY)
                Ecs.GetComponent<ECS::HierarchyComponent>(parent).Children.push_back(entity);
            return entity;
        }

        void MakeRoot()
        {
            Root = Add("Root", ECS::INVALID_ENTITY);
            Ecs.SetRoot(Root);
        }

        TestBody& Body(ECS::Entity entity) { return Ecs.GetComponent<TestBody>(entity); }
        const std::vector<ECS::Entity>& Children(ECS::Entity entity) { return Ecs.GetComponent<ECS::HierarchyComponent>(entity).Children; }
    };

    struct Fixture
    {
        EcsSerialization::ComponentSerializerRegistry Registry;
        TestPrefabs                                   Prefabs;

        Fixture()
        {
            Registry.RegisterReflected<TestBody>("TestBody");
            Registry.RegisterVisitable<TestTag>("TestTag");
            Registry.RegisterReflected<TestLink>("TestLink");
            Registry.SetPrefabProvider(&Prefabs);
            SetCar(1.0f, "body");
        }

        // car.prefab: Car (speed 2) > Wheel (speed), Door.
        void SetCar(float wheelSpeed, const std::string& doorLabel)
        {
            World source;
            const ECS::Entity car   = source.Add("Car", ECS::INVALID_ENTITY);
            const ECS::Entity wheel = source.Add("Wheel", car);
            const ECS::Entity door  = source.Add("Door", car);
            source.Ecs.AddComponent(car, TestBody{ 2.0f, "car" });
            source.Ecs.AddComponent(wheel, TestBody{ wheelSpeed, "wheel" });
            source.Ecs.AddComponent(door, TestBody{ 0.5f, doorLabel });
            Prefabs.Documents["car.prefab"] = std::make_shared<const YAML::Node>(
                EcsSerialization::EcsSerializer::SerializeSubtree(Registry, source.Ecs, car));
        }

        ECS::Entity Instantiate(World& world, ECS::Entity parent)
        {
            std::vector<ECS::Entity> entities;
            EcsSerialization::InstantiateOptions options;
            options.LocalEntities = &entities;
            const ECS::Entity root = EcsSerialization::EcsSerializer::InstantiateSubtree(
                Registry, world.Ecs, *Prefabs.Documents.at("car.prefab"), parent, "car.prefab", options);
            REQUIRE(root != ECS::INVALID_ENTITY);
            Prefabs.Link(world.Ecs, entities, "car.prefab");
            return root;
        }

        std::string Save(World& world) { return EcsSerialization::EcsSerializer::SerializeToString(Registry, world.Ecs, "Garage"); }

        void Load(World& world, const std::string& text)
        {
            std::string name;
            REQUIRE(EcsSerialization::EcsSerializer::DeserializeFromString(Registry, world.Ecs, name, text, "garage.yaml"));
        }
    };

    // A garage with two cars: the second's wheel overridden and tagged, an extra entity under the
    // first car's door, and a plain entity after them.
    struct Garage
    {
        World       Place;
        ECS::Entity First, Second, Extra, Lamp;

        explicit Garage(Fixture& fixture)
        {
            Place.MakeRoot();
            First  = fixture.Instantiate(Place, Place.Root);
            Second = fixture.Instantiate(Place, Place.Root);
            Lamp   = Place.Add("Lamp", Place.Root);
            Place.Ecs.AddComponent(Lamp, TestBody{ 9.0f, "lamp" });
            Extra = Place.Add("Extra", Place.Children(First)[1]);
            Place.Ecs.AddComponent(Extra, TestBody{ 7.0f, "extra" });
            const ECS::Entity wheel = Place.Children(Second)[0];
            Place.Body(wheel).Speed = 5.0f;
            Place.Ecs.AddComponent(wheel, TestTag{ 3.0f });
            Place.Body(Second).Speed = 4.0f;
        }
    };
}

TEST_CASE("Prefab scenes - an instance is written as its prefab, its ids, overrides and added entities")
{
    Fixture           fixture;
    Garage            garage(fixture);
    const std::string text = fixture.Save(garage.Place);

    CHECK(text.starts_with("Version: 2\n"));
    const YAML::Node scene = YAML::Load(text)["Scene"][0];
    const YAML::Node first = scene["Children"][0];
    CHECK(first["Prefab"].as<std::string>() == "car.prefab");
    CHECK(first["Entities"].as<std::vector<ECS::Entity>>() ==
          std::vector<ECS::Entity>{ garage.First, garage.Place.Children(garage.First)[0], garage.Place.Children(garage.First)[1] });
    CHECK_FALSE(first["Overrides"]);
    // Only the added entity is written, under the door it hangs from.
    REQUIRE(first["Children"].size() == 1);
    CHECK(first["Children"][0]["Name"].as<std::string>() == "Extra");
    CHECK(first["Children"][0]["Parent"].as<ECS::Entity>() == garage.Place.Children(garage.First)[1]);
    CHECK(text.find("Wheel") == std::string::npos);

    const EcsSerialization::OverrideSet overrides = EcsSerialization::ReadOverrides(scene["Children"][1]["Overrides"], "test");
    REQUIRE(overrides.size() == 3);
    CHECK((overrides[0].LocalId == 0 && overrides[0].Component == "TestBody" && overrides[0].Property == "Speed"));
    CHECK((overrides[1].LocalId == 1 && overrides[1].Property == "Speed" && overrides[1].Value.as<float>() == 5.0f));
    CHECK((overrides[2].LocalId == 1 && overrides[2].Component == "TestTag" && overrides[2].Property.empty()));
}

TEST_CASE("Prefab scenes - a scene with two instances, one overridden, loads back exactly")
{
    Fixture           fixture;
    Garage            garage(fixture);
    const std::string text = fixture.Save(garage.Place);

    World loaded;
    fixture.Load(loaded, text);
    CHECK(fixture.Save(loaded) == text);

    // Same ids, values, links and hierarchy.
    CHECK(loaded.Children(loaded.Ecs.GetRoot()) == std::vector<ECS::Entity>{ garage.First, garage.Second, garage.Lamp });
    const ECS::Entity wheel = loaded.Children(garage.Second)[0];
    CHECK(wheel == garage.Place.Children(garage.Second)[0]);
    CHECK(loaded.Body(wheel).Speed == 5.0f);
    CHECK(loaded.Ecs.GetComponent<TestTag>(wheel).Value == 3.0f);
    CHECK(loaded.Body(garage.Second).Speed == 4.0f);
    CHECK(loaded.Body(loaded.Children(garage.First)[0]).Speed == 1.0f);
    CHECK(loaded.Ecs.GetComponent<TestLink>(wheel).LocalId == 1);
    CHECK(loaded.Ecs.GetComponent<TestLink>(wheel).InstanceRoot == garage.Second);
    CHECK(loaded.Ecs.GetComponent<TestLink>(garage.Second).PrefabPath == "car.prefab");
    const ECS::Entity door = loaded.Children(garage.First)[1];
    CHECK(loaded.Children(door) == std::vector<ECS::Entity>{ garage.Extra });
    CHECK(loaded.Ecs.GetComponent<ECS::HierarchyComponent>(garage.Extra).Parent == door);
    CHECK(loaded.Body(garage.Lamp).Label == "lamp");
}

TEST_CASE("Prefab scenes - a changed prefab reaches the instances, and overrides survive")
{
    Fixture           fixture;
    Garage            garage(fixture);
    const std::string text = fixture.Save(garage.Place);

    fixture.SetCar(8.0f, "painted");
    World loaded;
    fixture.Load(loaded, text);
    CHECK(loaded.Body(loaded.Children(garage.First)[0]).Speed == 8.0f);  // not overridden: the new value
    CHECK(loaded.Body(loaded.Children(garage.Second)[0]).Speed == 5.0f); // overridden: kept
    CHECK(loaded.Body(loaded.Children(garage.First)[1]).Label == "painted");
    CHECK(loaded.Body(loaded.Children(garage.Second)[1]).Label == "painted");
}

TEST_CASE("Prefab scenes - overrides of nodes and properties the prefab lost are dropped with a warning")
{
    Fixture fixture;
    World   world;
    world.MakeRoot();
    const ECS::Entity car = fixture.Instantiate(world, world.Root);
    std::string       text = fixture.Save(world);
    // An override of a property no TestBody has, and of a node past the prefab's end.
    const std::string extra = "        Overrides:\n          - {LocalId: 1, Component: TestBody, Property: Grip, Value: 3}\n"
                              "          - {LocalId: 7, Component: TestBody, Property: Speed, Value: 3}\n"
                              "          - {LocalId: 1, Component: Nothing, Property: Speed, Value: 3}\n";
    const size_t      at    = text.find("        Children:\n          []");
    REQUIRE(at != std::string::npos);
    text.insert(at, extra);

    World      loaded;
    LogCapture log;
    fixture.Load(loaded, text);
    CHECK(log.Lines("TestBody.Grip on node 1 is dropped: the component has no such property").size() == 1);
    CHECK(log.Lines("TestBody.Speed on node 7 is dropped: the prefab has no such node").size() == 1);
    CHECK(log.Lines("Nothing.Speed on node 1 is dropped: no component is called that").size() == 1);
    CHECK(loaded.Body(loaded.Children(car)[0]).Speed == 1.0f);
}

TEST_CASE("Prefab scenes - a prefab that grew or shrank keeps the scene's other ids")
{
    Fixture fixture;
    World   world;
    world.MakeRoot();
    const ECS::Entity car  = fixture.Instantiate(world, world.Root);
    const ECS::Entity lamp = world.Add("Lamp", world.Root);
    const std::string text = fixture.Save(world);

    // Grown: a fourth node gets a fresh id that no saved entity uses.
    {
        World source;
        const ECS::Entity root = source.Add("Car", ECS::INVALID_ENTITY);
        for (const char* name : { "Wheel", "Door", "Roof" })
            source.Ecs.AddComponent(source.Add(name, root), TestBody{});
        source.Ecs.AddComponent(root, TestBody{});
        fixture.Prefabs.Documents["car.prefab"] =
            std::make_shared<const YAML::Node>(EcsSerialization::EcsSerializer::SerializeSubtree(fixture.Registry, source.Ecs, root));
        World loaded;
        fixture.Load(loaded, text);
        REQUIRE(loaded.Children(car).size() == 3);
        const ECS::Entity roof = loaded.Children(car)[2];
        CHECK(roof != lamp);
        CHECK(loaded.Ecs.GetComponent<ECS::HierarchyComponent>(lamp).Name == "Lamp");
        CHECK(loaded.Ecs.GetComponent<TestLink>(roof).LocalId == 3);
    }
    // Shrunk: the saved id of the lost node is not left alive.
    {
        World source;
        const ECS::Entity root = source.Add("Car", ECS::INVALID_ENTITY);
        source.Ecs.AddComponent(root, TestBody{});
        fixture.Prefabs.Documents["car.prefab"] =
            std::make_shared<const YAML::Node>(EcsSerialization::EcsSerializer::SerializeSubtree(fixture.Registry, source.Ecs, root));
        World loaded;
        fixture.Load(loaded, text);
        CHECK(loaded.Children(car).empty());
        CHECK(loaded.Ecs.GetEntityCount() == 3); // root, car, lamp
    }
}

TEST_CASE("Prefab scenes - a missing prefab loads as an empty game object keeping its added entities")
{
    Fixture fixture;
    Garage  garage(fixture);
    const std::string text = fixture.Save(garage.Place);
    fixture.Prefabs.Documents.clear();

    World      loaded;
    LogCapture log;
    fixture.Load(loaded, text);
    CHECK(log.Lines("instance 'Car' of car.prefab could not be instantiated").size() == 2);
    CHECK(loaded.Ecs.GetComponent<ECS::HierarchyComponent>(garage.First).Name == "Car");
    // The door is gone, so the extra entity hangs from the instance's root.
    CHECK(loaded.Children(garage.First) == std::vector<ECS::Entity>{ garage.Extra });
    CHECK(loaded.Ecs.GetComponent<ECS::HierarchyComponent>(garage.Extra).Parent == garage.First);
    CHECK(loaded.Ecs.GetEntityCount() == 5); // root, two cars, lamp, extra
}

TEST_CASE("Prefab scenes - a scene without instances is written as version 1, as before")
{
    Fixture fixture;
    World   world;
    world.MakeRoot();
    const ECS::Entity box = world.Add("Box", world.Root);
    world.Ecs.AddComponent(box, TestBody{ 3.0f, "box" });

    const std::string with = fixture.Save(world);
    fixture.Registry.SetPrefabProvider(nullptr);
    CHECK(fixture.Save(world) == with);
    CHECK(with.starts_with("Version: 1\n"));
}

TEST_CASE("OverrideSet - values compare as written by the emitter or by hand")
{
    CHECK(EcsSerialization::ValuesEqual(YAML::Load("0.15"), YAML::Load("0.150000006")));
    CHECK(EcsSerialization::ValuesEqual(YAML::Load("[1, 2.0, a]"), YAML::Load("[1.0, 2, a]")));
    CHECK(EcsSerialization::ValuesEqual(YAML::Load("{x: 1, y: [2]}"), YAML::Load("{y: [2.0], x: 1}")));
    CHECK_FALSE(EcsSerialization::ValuesEqual(YAML::Load("0.15"), YAML::Load("0.16")));
    CHECK_FALSE(EcsSerialization::ValuesEqual(YAML::Load("[1, 2]"), YAML::Load("[1, 2, 3]")));
    CHECK_FALSE(EcsSerialization::ValuesEqual(YAML::Load("a"), YAML::Load("[a]")));
    CHECK_FALSE(EcsSerialization::ValuesEqual(YAML::Load("{x: 1}"), YAML::Load("{y: 1}")));
}

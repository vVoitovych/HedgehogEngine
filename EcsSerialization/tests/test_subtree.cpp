#include "doctest/doctest/doctest.h"

#include "EcsSerialization/api/EcsSerializer.hpp"
#include "EcsSerialization/api/ComponentSerializerRegistry.hpp"

#include "yaml-cpp/yaml.h"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <algorithm>
#include <span>
#include <string>
#include <vector>

namespace
{
    // A reflected component naming other entities: Target is an EntityRef, Note a plain id.
    struct TestLink
    {
        ECS::Entity Target = ECS::INVALID_ENTITY;
        ECS::Entity Note   = ECS::INVALID_ENTITY;
        float       Weight = 0.0f;

        static void* TargetAccessor(void* c) { return &static_cast<TestLink*>(c)->Target; }
        static void* NoteAccessor(void* c) { return &static_cast<TestLink*>(c)->Note; }
        static void* WeightAccessor(void* c) { return &static_cast<TestLink*>(c)->Weight; }

        static std::span<const Reflection::PropertyDescriptor> GetProperties()
        {
            static const Reflection::PropertyDescriptor properties[] = {
                { Reflection::TypeTag::Entity, "Target", TargetAccessor, Reflection::PropertyFlags::EntityRef, 0.0f, 0.0f },
                { Reflection::TypeTag::Entity, "Note", NoteAccessor, Reflection::PropertyFlags::None, 0.0f, 0.0f },
                { Reflection::TypeTag::Float, "Weight", WeightAccessor, Reflection::PropertyFlags::None, 0.0f, 0.0f },
            };
            return properties;
        }
    };

    // Entity references a hand-written serializer keeps, as the script component's are.
    struct TestTargets
    {
        std::vector<ECS::Entity> Entities;
    };

    struct World
    {
        ECS::ECS                                      Ecs;
        EcsSerialization::ComponentSerializerRegistry Registry;
        ECS::Entity                                   Root = ECS::INVALID_ENTITY;

        World()
        {
            Ecs.Init();
            Ecs.RegisterComponent<ECS::HierarchyComponent>();
            Ecs.RegisterComponent<TestLink>();
            Ecs.RegisterComponent<TestTargets>();
            Registry.RegisterReflected<TestLink>("TestLink");
            Registry.RegisterCustom(
                "TestTargets",
                [](YAML::Emitter& out, const ECS::ECS& ecs, ECS::Entity e)
                { out << YAML::Key << "TestTargets" << YAML::Value << YAML::Flow << ecs.GetComponent<TestTargets>(e).Entities; },
                [](ECS::ECS& ecs, ECS::Entity e, const YAML::Node& node)
                { ecs.AddComponent(e, TestTargets{ node.as<std::vector<ECS::Entity>>() }); },
                [](const ECS::ECS& ecs, ECS::Entity e) { return ecs.HasComponent<TestTargets>(e); },
                [](ECS::ECS& ecs, ECS::Entity e, const EcsSerialization::EntityRemap& remap)
                {
                    for (ECS::Entity& target : ecs.GetComponent<TestTargets>(e).Entities)
                        target = remap(target);
                });

            Root = Ecs.CreateEntity();
            Ecs.AddComponent(Root, ECS::HierarchyComponent{ "Root", ECS::INVALID_ENTITY, {} });
            Ecs.SetRoot(Root);
        }

        ECS::Entity Add(const std::string& name, ECS::Entity parent)
        {
            const ECS::Entity entity = Ecs.CreateEntity();
            Ecs.AddComponent(entity, ECS::HierarchyComponent{ name, parent, {} });
            Ecs.GetComponent<ECS::HierarchyComponent>(parent).Children.push_back(entity);
            return entity;
        }

        const ECS::HierarchyComponent& Hierarchy(ECS::Entity entity) { return Ecs.GetComponent<ECS::HierarchyComponent>(entity); }

        ECS::Entity Instantiate(const YAML::Node& document, ECS::Entity parent)
        {
            return EcsSerialization::EcsSerializer::InstantiateSubtree(Registry, Ecs, document, parent, "test subtree");
        }
    };

    // Root > Car (Wheel A, Wheel B), plus Garage outside the car.
    struct CarWorld : World
    {
        ECS::Entity Garage = Add("Garage", Root);
        ECS::Entity Car    = Add("Car", Root);
        ECS::Entity WheelA = Add("Wheel A", Car);
        ECS::Entity WheelB = Add("Wheel B", Car);
    };
}

TEST_CASE("Subtree - the document holds dense local ids and the source ids")
{
    CarWorld world;
    const YAML::Node document = EcsSerialization::EcsSerializer::SerializeSubtree(world.Registry, world.Ecs, world.Car);

    CHECK(document["Version"].as<int>() == EcsSerialization::EcsSerializer::BASE_FORMAT_VERSION);
    CHECK(document["SourceIds"].as<std::vector<ECS::Entity>>() == std::vector<ECS::Entity>{ world.Car, world.WheelA, world.WheelB });
    const YAML::Node car = document["Subtree"][0];
    CHECK(car["Entity"].as<ECS::Entity>() == 0);
    CHECK(car["Name"].as<std::string>() == "Car");
    CHECK(car["Parent"].as<ECS::Entity>() == ECS::INVALID_ENTITY);
    REQUIRE(car["Children"].size() == 2);
    CHECK(car["Children"][0]["Entity"].as<ECS::Entity>() == 1);
    CHECK(car["Children"][0]["Parent"].as<ECS::Entity>() == 0);
    CHECK(car["Children"][1]["Name"].as<std::string>() == "Wheel B");
    CHECK(car["Children"][1]["Entity"].as<ECS::Entity>() == 2);
}

TEST_CASE("Subtree - instantiating twice gives two disjoint hierarchies under their parents")
{
    CarWorld world;
    world.Ecs.AddComponent(world.WheelB, TestLink{ ECS::INVALID_ENTITY, ECS::INVALID_ENTITY, 2.5f });
    const YAML::Node document = EcsSerialization::EcsSerializer::SerializeSubtree(world.Registry, world.Ecs, world.Car);
    const size_t     before   = world.Ecs.GetEntityCount();

    const ECS::Entity first  = world.Instantiate(document, world.Root);
    const ECS::Entity second = world.Instantiate(document, world.Garage);
    REQUIRE(first != ECS::INVALID_ENTITY);
    REQUIRE(second != ECS::INVALID_ENTITY);
    CHECK(world.Ecs.GetEntityCount() == before + 6);

    CHECK(world.Hierarchy(world.Root).Children.back() == first);
    CHECK(world.Hierarchy(world.Garage).Children == std::vector<ECS::Entity>{ second });
    CHECK(world.Hierarchy(first).Parent == world.Root);
    CHECK(world.Hierarchy(second).Parent == world.Garage);

    std::vector<ECS::Entity> seen{ world.Root, world.Garage, world.Car, world.WheelA, world.WheelB };
    for (const ECS::Entity copy : { first, second })
    {
        const auto& car = world.Hierarchy(copy);
        CHECK(car.Name == "Car");
        REQUIRE(car.Children.size() == 2);
        CHECK(world.Hierarchy(car.Children[0]).Name == "Wheel A");
        CHECK(world.Hierarchy(car.Children[1]).Name == "Wheel B");
        for (const ECS::Entity child : car.Children)
            CHECK(world.Hierarchy(child).Parent == copy);
        CHECK(world.Ecs.GetComponent<TestLink>(car.Children[1]).Weight == 2.5f);
        CHECK_FALSE(world.Ecs.HasComponent<TestLink>(car.Children[0]));

        for (const ECS::Entity entity : { copy, car.Children[0], car.Children[1] })
        {
            CHECK(std::find(seen.begin(), seen.end(), entity) == seen.end());
            seen.push_back(entity);
        }
    }
    // The source is untouched.
    CHECK(world.Hierarchy(world.Car).Children == std::vector<ECS::Entity>{ world.WheelA, world.WheelB });
}

TEST_CASE("Subtree - internal references point at the copies and external ones are kept")
{
    CarWorld world;
    const ECS::Entity doomed = world.Add("Doomed", world.Root);
    // Wheel A names its sibling (internal), the garage (external) and the doomed entity; the
    // car's hand-written list names its own wheel, the garage and nothing.
    world.Ecs.AddComponent(world.WheelA, TestLink{ world.WheelB, world.WheelB, 1.0f });
    world.Ecs.AddComponent(world.WheelB, TestLink{ world.Garage, ECS::INVALID_ENTITY, 1.0f });
    world.Ecs.AddComponent(world.Car, TestTargets{ { world.WheelA, world.Garage, doomed, ECS::INVALID_ENTITY } });
    const YAML::Node document = EcsSerialization::EcsSerializer::SerializeSubtree(world.Registry, world.Ecs, world.Car);

    world.Ecs.GetComponent<ECS::HierarchyComponent>(world.Root).Children.pop_back();
    world.Ecs.DestroyEntity(doomed);

    const ECS::Entity copy = world.Instantiate(document, world.Root);
    REQUIRE(copy != ECS::INVALID_ENTITY);
    const ECS::Entity wheelA = world.Hierarchy(copy).Children[0];
    const ECS::Entity wheelB = world.Hierarchy(copy).Children[1];

    const auto& linkA = world.Ecs.GetComponent<TestLink>(wheelA);
    CHECK(linkA.Target == wheelB);
    // Note is not flagged EntityRef, so it keeps the id it was written with.
    CHECK(linkA.Note == world.WheelB);
    CHECK(world.Ecs.GetComponent<TestLink>(wheelB).Target == world.Garage);

    // The doomed entity's id is the copy's first, recycled: it must not point at the copy.
    const auto& targets = world.Ecs.GetComponent<TestTargets>(copy).Entities;
    CHECK(targets == std::vector<ECS::Entity>{ wheelA, world.Garage, ECS::INVALID_ENTITY, ECS::INVALID_ENTITY });

    // The source still names its own entities.
    CHECK(world.Ecs.GetComponent<TestLink>(world.WheelA).Target == world.WheelB);
}

TEST_CASE("Subtree - a bad document or parent creates nothing")
{
    CarWorld   world;
    const auto valid  = EcsSerialization::EcsSerializer::SerializeSubtreeToString(world.Registry, world.Ecs, world.Car);
    const auto before = world.Ecs.GetEntityCount();

    struct Case
    {
        const char* Name;
        std::string Text;
        const char* Expected;
    };
    auto replace = [&valid](const std::string& from, const std::string& to)
    {
        std::string text = valid;
        const size_t at  = text.find(from);
        REQUIRE(at != std::string::npos);
        return text.replace(at, from.size(), to);
    };
    const std::vector<Case> cases = {
        { "newer version", replace("Version: 1", "Version: 9"), "is format version 9" },
        { "too few source ids", replace("SourceIds: [" + std::to_string(world.Car) + ", ", "SourceIds: ["), "SourceIds lists 2 entities" },
        { "duplicate local id", replace("Entity: 2", "Entity: 1"), "local id 1 is used twice" },
        { "local id out of range", replace("Entity: 2", "Entity: 7"), "local id 7 is not below" },
        { "no subtree", "Version: 1\nSourceIds: []\n", "Subtree is not a sequence" },
        { "malformed name", replace("Name: Wheel B", "Name: [Wheel, B]"), "Failed to instantiate" },
    };
    for (const Case& c : cases)
    {
        CAPTURE(c.Name);
        LogCapture        log;
        const ECS::Entity result = world.Instantiate(YAML::Load(c.Text), world.Root);
        CHECK(result == ECS::INVALID_ENTITY);
        CHECK(log.Text().find(c.Expected) != std::string::npos);
        CHECK(world.Ecs.GetEntityCount() == before);
        CHECK(world.Hierarchy(world.Root).Children.size() == 2);
    }

    LogCapture log;
    CHECK(world.Instantiate(YAML::Load(valid), ECS::MAX_ENTITIES - 1) == ECS::INVALID_ENTITY);
    CHECK(log.Text().find("has no hierarchy") != std::string::npos);
}

TEST_CASE("Subtree - an instance that does not fit in MAX_ENTITIES is refused")
{
    CarWorld world;
    const YAML::Node document = EcsSerialization::EcsSerializer::SerializeSubtree(world.Registry, world.Ecs, world.Car);
    while (world.Ecs.GetEntityCount() < ECS::MAX_ENTITIES - 2)
        world.Ecs.CreateEntity();

    LogCapture log;
    CHECK(world.Instantiate(document, world.Root) == ECS::INVALID_ENTITY);
    CHECK(log.Text().find("do not fit") != std::string::npos);
    CHECK(world.Ecs.GetEntityCount() == ECS::MAX_ENTITIES - 2);
}

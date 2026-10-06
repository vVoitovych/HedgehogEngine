#include "doctest/doctest/doctest.h"

#include "EcsSerialization/api/ComponentTypeRegistry.hpp"
#include "EcsSerialization/api/EcsSerializer.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include "yaml-cpp/yaml.h"

#include <span>
#include <string>
#include <vector>

using EcsSerialization::ComponentDesc;
using EcsSerialization::ComponentInfo;
using EcsSerialization::ComponentTypeRegistry;

namespace
{
    // A reflected component with an enabled flag.
    struct TestLamp
    {
        bool  Enabled   = true;
        float Intensity = 1.0f;

        static void* EnabledAccessor(void* c) { return &static_cast<TestLamp*>(c)->Enabled; }
        static void* IntensityAccessor(void* c) { return &static_cast<TestLamp*>(c)->Intensity; }

        static std::span<const Reflection::PropertyDescriptor> GetProperties()
        {
            static const Reflection::PropertyDescriptor properties[] = {
                { Reflection::TypeTag::Bool, "Enabled", EnabledAccessor, Reflection::PropertyFlags::None, 0.0f, 0.0f },
                { Reflection::TypeTag::Float, "Intensity", IntensityAccessor, Reflection::PropertyFlags::None, 0.0f, 0.0f },
            };
            return properties;
        }
    };

    // A component with a hand-written serializer.
    struct TestTag
    {
        std::string Text = "tag";
    };

    // Never registered in the ECS by the fixture.
    struct TestSpare
    {
        int Value = 0;
    };

    void RegisterTestTagSerializer(EcsSerialization::ComponentSerializerRegistry& serializers)
    {
        serializers.RegisterCustom(
            "TestTag",
            [](YAML::Emitter& out, const ECS::ECS& ecs, ECS::Entity e)
            { out << YAML::Key << "TestTag" << YAML::Value << ecs.GetComponent<TestTag>(e).Text; },
            [](ECS::ECS& ecs, ECS::Entity e, const YAML::Node& node) { ecs.AddComponent(e, TestTag{ node.as<std::string>() }); },
            [](const ECS::ECS& ecs, ECS::Entity e) { return ecs.HasComponent<TestTag>(e); });
    }

    struct World
    {
        ECS::ECS                                      Ecs;
        EcsSerialization::ComponentSerializerRegistry Serializers;
        ComponentTypeRegistry                         Types{ Ecs, Serializers };
        ECS::Entity                                   Root = ECS::INVALID_ENTITY;

        // Without a root, the world is empty for a scene to load into.
        explicit World(bool withRoot = true)
        {
            Ecs.Init();
            REQUIRE(Types.RegisterUnserialized<ECS::HierarchyComponent>(
                ComponentDesc{ .Key = "Hierarchy", .DisplayName = "Hierarchy", .Addable = false, .Removable = false }));
            REQUIRE(Types.RegisterReflected<TestLamp>(ComponentDesc{
                .Key = "TestLamp", .DisplayName = "Lamp", .Category = "Lights", .Icon = "light", .EnabledProperty = "Enabled" }));
            REQUIRE(Types.RegisterCustom<TestTag>(ComponentDesc{ .Key = "TestTag", .DisplayName = "Tag" }, RegisterTestTagSerializer));

            if (!withRoot)
                return;
            Root = Ecs.CreateEntity();
            Ecs.AddComponent(Root, ECS::HierarchyComponent{ "Root", ECS::INVALID_ENTITY, {} });
            Ecs.SetRoot(Root);
        }

        ECS::Entity Add(const std::string& name)
        {
            const ECS::Entity entity = Ecs.CreateEntity();
            Ecs.AddComponent(entity, ECS::HierarchyComponent{ name, Root, {} });
            Ecs.GetComponent<ECS::HierarchyComponent>(Root).Children.push_back(entity);
            return entity;
        }
    };
}

TEST_CASE("ComponentTypeRegistry - types are listed in registration order, known to the ECS and the serializers")
{
    World world;
    REQUIRE(world.Types.GetInfos().size() == 3);
    CHECK(world.Types.GetInfos()[0].Key == "Hierarchy");
    CHECK(world.Types.GetInfos()[1].Key == "TestLamp");
    CHECK(world.Types.GetInfos()[2].Key == "TestTag");

    CHECK(world.Ecs.IsComponentRegistered<ECS::HierarchyComponent>());
    CHECK(world.Ecs.IsComponentRegistered<TestLamp>());
    CHECK(world.Ecs.IsComponentRegistered<TestTag>());
    CHECK(world.Serializers.FindHandler("Hierarchy") == nullptr);
    CHECK(world.Serializers.FindHandler("TestLamp") != nullptr);
    CHECK(world.Serializers.FindHandler("TestTag") != nullptr);

    const ComponentInfo* lamp = world.Types.Find("TestLamp");
    REQUIRE(lamp != nullptr);
    CHECK(lamp->DisplayName == "Lamp");
    CHECK(lamp->Category == "Lights");
    CHECK(lamp->Icon == "light");
    CHECK(lamp->Properties.size() == 2);
    CHECK(world.Types.Find("TestTag")->Properties.empty());
    CHECK_FALSE(world.Types.Find("Hierarchy")->Addable);
    CHECK(world.Types.Find("Missing") == nullptr);
}

TEST_CASE("ComponentTypeRegistry - AddDefault, Has, Get, Enabled and Remove work type-erased")
{
    World                world;
    const ECS::Entity    entity = world.Add("Thing");
    const ComponentInfo& lamp   = *world.Types.Find("TestLamp");

    CHECK_FALSE(lamp.Has(world.Ecs, entity));
    lamp.AddDefault(world.Ecs, entity);
    REQUIRE(lamp.Has(world.Ecs, entity));
    void* component = lamp.Get(world.Ecs, entity);
    CHECK(component == &world.Ecs.GetComponent<TestLamp>(entity));
    CHECK(static_cast<TestLamp*>(component)->Intensity == 1.0f);

    bool* enabled = lamp.Enabled(component);
    REQUIRE(enabled == &world.Ecs.GetComponent<TestLamp>(entity).Enabled);
    *enabled = false;
    CHECK_FALSE(world.Ecs.GetComponent<TestLamp>(entity).Enabled);
    TestTag tag;
    CHECK(world.Types.Find("TestTag")->Enabled(&tag) == nullptr);

    lamp.Remove(world.Ecs, entity);
    CHECK_FALSE(lamp.Has(world.Ecs, entity));
}

TEST_CASE("ComponentTypeRegistry - a desc's AddDefault replaces T{}")
{
    ECS::ECS                                      ecs;
    EcsSerialization::ComponentSerializerRegistry serializers;
    ComponentTypeRegistry                         types(ecs, serializers);
    ecs.Init();
    REQUIRE(types.RegisterReflected<TestLamp>(ComponentDesc{
        .Key = "TestLamp", .AddDefault = [](ECS::ECS& e, ECS::Entity entity) { e.AddComponent(entity, TestLamp{ true, 5.0f }); } }));

    const ECS::Entity entity = ecs.CreateEntity();
    types.Find("TestLamp")->AddDefault(ecs, entity);
    CHECK(ecs.GetComponent<TestLamp>(entity).Intensity == 5.0f);
}

TEST_CASE("ComponentTypeRegistry - registered components round-trip through EcsSerializer")
{
    World             world;
    const ECS::Entity thing = world.Add("Thing");
    world.Ecs.AddComponent(thing, TestLamp{ false, 2.5f });
    world.Ecs.AddComponent(thing, TestTag{ "hello" });
    const std::string text = EcsSerialization::EcsSerializer::SerializeToString(world.Serializers, world.Ecs, "Scene");

    World       loaded(false);
    std::string name;
    REQUIRE(EcsSerialization::EcsSerializer::DeserializeFromString(loaded.Serializers, loaded.Ecs, name, text, "test"));
    REQUIRE(loaded.Ecs.HasComponent<TestLamp>(thing));
    CHECK_FALSE(loaded.Ecs.GetComponent<TestLamp>(thing).Enabled);
    CHECK(loaded.Ecs.GetComponent<TestLamp>(thing).Intensity == 2.5f);
    CHECK(loaded.Ecs.GetComponent<TestTag>(thing).Text == "hello");
    CHECK(loaded.Ecs.GetComponent<ECS::HierarchyComponent>(thing).Name == "Thing");
    CHECK(EcsSerialization::EcsSerializer::SerializeToString(loaded.Serializers, loaded.Ecs, name) == text);
}

TEST_CASE("ComponentTypeRegistry - Unregister takes the type out of the ECS, the serializers and the list")
{
    World             world;
    const ECS::Entity thing = world.Add("Thing");
    world.Ecs.AddComponent(thing, TestLamp{});

    REQUIRE(world.Types.Unregister("TestLamp"));
    CHECK_FALSE(world.Ecs.IsComponentRegistered<TestLamp>());
    CHECK(world.Serializers.FindHandler("TestLamp") == nullptr);
    CHECK(world.Types.Find("TestLamp") == nullptr);
    REQUIRE(world.Types.GetInfos().size() == 2);
    CHECK(world.Types.GetInfos()[1].Key == "TestTag");

    CHECK_FALSE(world.Types.Unregister("TestLamp"));
    CHECK_FALSE(world.Types.Unregister("Missing"));

    // Registered again, as a plugin reloading would.
    CHECK(world.Types.RegisterReflected<TestLamp>(ComponentDesc{ .Key = "TestLamp" }));
    CHECK(world.Types.GetInfos().back().Key == "TestLamp");
}

TEST_CASE("ComponentTypeRegistry - Unregister is refused while a system requires the type")
{
    struct LampSystem : ECS::System
    {
    };

    World world;
    world.Ecs.RegisterSystem<LampSystem>();
    ECS::Signature signature;
    signature.set(world.Ecs.GetComponentType<TestLamp>());
    world.Ecs.SetSystemSignature<LampSystem>(signature);

    CHECK_FALSE(world.Types.Unregister("TestLamp"));
    CHECK(world.Types.Find("TestLamp") != nullptr);
    CHECK(world.Serializers.FindHandler("TestLamp") != nullptr);
}

TEST_CASE("ComponentTypeRegistry - bad registrations are refused with one error and change nothing")
{
    World      world;
    LogCapture log;

    SUBCASE("a key used twice")
    {
        CHECK_FALSE(world.Types.RegisterUnserialized<TestSpare>(ComponentDesc{ .Key = "TestLamp" }));
        CHECK(log.Lines("'TestLamp' is not registered: the key is already registered").size() == 1);
    }
    SUBCASE("an empty key")
    {
        CHECK_FALSE(world.Types.RegisterUnserialized<TestSpare>(ComponentDesc{}));
        CHECK(log.Lines("the key is empty").size() == 1);
    }
    SUBCASE("a type the ECS already has")
    {
        CHECK_FALSE(world.Types.RegisterReflected<TestLamp>(ComponentDesc{ .Key = "OtherLamp" }));
        CHECK(log.Lines("the ECS already has the type").size() == 1);
    }
    SUBCASE("an enabled property that is not a reflected bool")
    {
        CHECK_FALSE(world.Types.RegisterUnserialized<TestSpare>(ComponentDesc{ .Key = "Spare", .EnabledProperty = "Value" }));
        CHECK(log.Lines("EnabledProperty 'Value' is not a reflected bool property").size() == 1);
    }
    SUBCASE("a custom serializer that adds no handler of the key")
    {
        CHECK_FALSE(world.Types.RegisterCustom<TestSpare>(ComponentDesc{ .Key = "Spare" },
                                                          [](EcsSerialization::ComponentSerializerRegistry&) {}));
        CHECK(log.Lines("its serializer added no handler of that key").size() == 1);
    }

    CHECK_FALSE(world.Ecs.IsComponentRegistered<TestSpare>());
    CHECK(world.Types.Find("Spare") == nullptr);
    CHECK(world.Types.GetInfos().size() == 3);
}

TEST_CASE("ComponentTypeRegistry - GetInfosInOrder sorts by SortOrder, ties in registration order")
{
    ECS::ECS                                      ecs;
    EcsSerialization::ComponentSerializerRegistry serializers;
    ComponentTypeRegistry                         types(ecs, serializers);
    ecs.Init();
    REQUIRE(types.RegisterReflected<TestLamp>(ComponentDesc{ .Key = "TestLamp", .SortOrder = 20 }));
    REQUIRE(types.RegisterCustom<TestTag>(ComponentDesc{ .Key = "TestTag", .SortOrder = 10 }, RegisterTestTagSerializer));
    REQUIRE(types.RegisterUnserialized<TestSpare>(ComponentDesc{ .Key = "TestSpare", .SortOrder = 10 }));

    std::vector<std::string> keys;
    for (const ComponentInfo* info : types.GetInfosInOrder())
        keys.push_back(info->Key);
    CHECK(keys == std::vector<std::string>{ "TestTag", "TestSpare", "TestLamp" });
    // Registration order is untouched: it is the order serializers write in.
    CHECK(types.GetInfos().front().Key == "TestLamp");
}

namespace
{
    // A reflected component with one property of each tag a plugin component typically uses.
    struct TestRich
    {
        float       Speed  = 0.0f;
        int32_t     Count  = 0;
        bool        Active = false;
        std::string Label;
        HM::Vector3 Axis   = HM::Vector3(0.0f, 0.0f, 0.0f);
        ECS::Entity Target = ECS::INVALID_ENTITY;

        static void* SpeedAccessor(void* c) { return &static_cast<TestRich*>(c)->Speed; }
        static void* CountAccessor(void* c) { return &static_cast<TestRich*>(c)->Count; }
        static void* ActiveAccessor(void* c) { return &static_cast<TestRich*>(c)->Active; }
        static void* LabelAccessor(void* c) { return &static_cast<TestRich*>(c)->Label; }
        static void* AxisAccessor(void* c) { return &static_cast<TestRich*>(c)->Axis; }
        static void* TargetAccessor(void* c) { return &static_cast<TestRich*>(c)->Target; }

        static std::span<const Reflection::PropertyDescriptor> GetProperties()
        {
            using Reflection::PropertyFlags;
            using Reflection::TypeTag;
            static const Reflection::PropertyDescriptor properties[] = {
                { TypeTag::Float, "Speed", SpeedAccessor, PropertyFlags::None, 0.0f, 0.0f },
                { TypeTag::Int, "Count", CountAccessor, PropertyFlags::None, 0.0f, 0.0f },
                { TypeTag::Bool, "Active", ActiveAccessor, PropertyFlags::None, 0.0f, 0.0f },
                { TypeTag::String, "Label", LabelAccessor, PropertyFlags::None, 0.0f, 0.0f },
                { TypeTag::Vec3, "Axis", AxisAccessor, PropertyFlags::None, 0.0f, 0.0f },
                { TypeTag::Entity, "Target", TargetAccessor, PropertyFlags::EntityRef, 0.0f, 0.0f },
            };
            return properties;
        }
    };

    // A world whose ECS keeps unknown components, as the engine's does: the store and the hierarchy
    // registered, the rich component not yet.
    struct KeepingWorld
    {
        ECS::ECS                                      Ecs;
        EcsSerialization::ComponentSerializerRegistry Serializers;
        ComponentTypeRegistry                         Types{ Ecs, Serializers };
        ECS::Entity                                   Root = ECS::INVALID_ENTITY;

        // Without a root, the world is empty for a scene to load into.
        explicit KeepingWorld(bool withRoot = true)
        {
            Ecs.Init();
            REQUIRE(Types.RegisterUnserialized<EcsSerialization::UnknownComponentsComponent>(ComponentDesc{
                .Key = EcsSerialization::UNKNOWN_COMPONENTS_KEY, .Addable = false, .Removable = false, .Inspectable = false }));
            REQUIRE(Types.RegisterUnserialized<ECS::HierarchyComponent>(ComponentDesc{ .Key = "Hierarchy" }));
            if (!withRoot)
                return;
            Root = Ecs.CreateEntity();
            Ecs.AddComponent(Root, ECS::HierarchyComponent{ "Root", ECS::INVALID_ENTITY, {} });
            Ecs.SetRoot(Root);
        }

        ECS::Entity Add(const std::string& name)
        {
            const ECS::Entity entity = Ecs.CreateEntity();
            Ecs.AddComponent(entity, ECS::HierarchyComponent{ name, Root, {} });
            Ecs.GetComponent<ECS::HierarchyComponent>(Root).Children.push_back(entity);
            return entity;
        }

        bool RegisterRich() { return Types.RegisterReflected<TestRich>(ComponentDesc{ .Key = "TestRich" }); }

        size_t KeptCount(ECS::Entity entity)
        {
            return Ecs.HasComponent<EcsSerialization::UnknownComponentsComponent>(entity)
                       ? Ecs.GetComponent<EcsSerialization::UnknownComponentsComponent>(entity).Entries.size()
                       : 0;
        }

        std::string Save() { return EcsSerialization::EcsSerializer::SerializeToString(Serializers, Ecs, "Scene"); }
    };

    TestRich MakeRich(ECS::Entity target)
    {
        TestRich rich;
        rich.Speed  = 2.5f;
        rich.Count  = -7;
        rich.Active = true;
        rich.Label  = "spin me";
        rich.Axis   = HM::Vector3(0.0f, 1.0f, 0.5f);
        rich.Target = target;
        return rich;
    }

    void CheckRich(const TestRich& rich, ECS::Entity target)
    {
        CHECK(rich.Speed == 2.5f);
        CHECK(rich.Count == -7);
        CHECK(rich.Active);
        CHECK(rich.Label == "spin me");
        CHECK(rich.Axis.y() == 1.0f);
        CHECK(rich.Axis.z() == 0.5f);
        CHECK(rich.Target == target);
    }
}

TEST_CASE("ComponentTypeRegistry - registering a type adopts the data a scene kept for it")
{
    KeepingWorld      world;
    const ECS::Entity thing = world.Add("Thing");
    const ECS::Entity other = world.Add("Other");
    REQUIRE(world.RegisterRich());
    world.Ecs.AddComponent(thing, MakeRich(other));
    const std::string withRich = world.Save();

    // Loaded where the type is not registered (its plugin missing): the data is kept.
    KeepingWorld missing(false);
    std::string  name;
    {
        LogCapture log;
        REQUIRE(EcsSerialization::EcsSerializer::DeserializeFromString(missing.Serializers, missing.Ecs, name, withRich, "test"));
        CHECK(log.Lines("component 'TestRich' has no registered type").size() == 1);
    }
    CHECK(missing.KeptCount(thing) == 1);

    // Registered now: the entity gets the component with the saved values, and the store goes.
    REQUIRE(missing.RegisterRich());
    REQUIRE(missing.Ecs.HasComponent<TestRich>(thing));
    CheckRich(missing.Ecs.GetComponent<TestRich>(thing), other);
    CHECK_FALSE(missing.Ecs.HasComponent<EcsSerialization::UnknownComponentsComponent>(thing));
    CHECK(missing.Save() == withRich);
}

TEST_CASE("ComponentTypeRegistry - Unregister with Keep keeps the data, and registering again restores it")
{
    KeepingWorld      world;
    const ECS::Entity thing = world.Add("Thing");
    const ECS::Entity other = world.Add("Other");
    REQUIRE(world.RegisterRich());
    world.Ecs.AddComponent(thing, MakeRich(other));
    const std::string withRich = world.Save();

    REQUIRE(world.Types.Unregister("TestRich", EcsSerialization::UnknownData::Keep));
    CHECK_FALSE(world.Ecs.IsComponentRegistered<TestRich>());
    CHECK(world.KeptCount(thing) == 1);
    CHECK(world.KeptCount(other) == 0);
    // A scene saved meanwhile still holds the data, written as before.
    CHECK(world.Save() == withRich);

    REQUIRE(world.RegisterRich());
    CheckRich(world.Ecs.GetComponent<TestRich>(thing), other);
    CHECK(world.KeptCount(thing) == 0);
}

TEST_CASE("ComponentTypeRegistry - Unregister without Keep drops the data as before")
{
    KeepingWorld      world;
    const ECS::Entity thing = world.Add("Thing");
    REQUIRE(world.RegisterRich());
    world.Ecs.AddComponent(thing, MakeRich(thing));

    REQUIRE(world.Types.Unregister("TestRich"));
    CHECK(world.KeptCount(thing) == 0);
    REQUIRE(world.RegisterRich());
    CHECK_FALSE(world.Ecs.HasComponent<TestRich>(thing));
}

TEST_CASE("ComponentTypeRegistry - a refused Unregister with Keep leaves no kept copy")
{
    struct RichSystem : ECS::System
    {
    };

    KeepingWorld      world;
    const ECS::Entity thing = world.Add("Thing");
    REQUIRE(world.RegisterRich());
    world.Ecs.AddComponent(thing, MakeRich(thing));
    world.Ecs.RegisterSystem<RichSystem>();
    ECS::Signature signature;
    signature.set(world.Ecs.GetComponentType<TestRich>());
    world.Ecs.SetSystemSignature<RichSystem>(signature);

    CHECK_FALSE(world.Types.Unregister("TestRich", EcsSerialization::UnknownData::Keep));
    CHECK(world.Ecs.HasComponent<TestRich>(thing));
    CHECK(world.KeptCount(thing) == 0);
}

TEST_CASE("ComponentTypeRegistry - a custom component adopts and keeps its data the same way")
{
    KeepingWorld      world;
    const ECS::Entity thing = world.Add("Thing");
    REQUIRE(world.Types.RegisterCustom<TestTag>(ComponentDesc{ .Key = "TestTag" }, RegisterTestTagSerializer));
    world.Ecs.AddComponent(thing, TestTag{ "kept" });

    REQUIRE(world.Types.Unregister("TestTag", EcsSerialization::UnknownData::Keep));
    CHECK(world.KeptCount(thing) == 1);
    REQUIRE(world.Types.RegisterCustom<TestTag>(ComponentDesc{ .Key = "TestTag" }, RegisterTestTagSerializer));
    CHECK(world.Ecs.GetComponent<TestTag>(thing).Text == "kept");
    CHECK(world.KeptCount(thing) == 0);
}

TEST_CASE("ComponentTypeRegistry - reserved keys are refused")
{
    KeepingWorld world;
    LogCapture   log;
    for (const char* key : { "Name", "Children", "Prefab", EcsSerialization::UNKNOWN_COMPONENTS_KEY })
    {
        CHECK_FALSE(world.Types.RegisterUnserialized<TestSpare>(ComponentDesc{ .Key = key }));
        CHECK(log.Lines(std::string("'") + key + "' is not registered: the key is reserved").size() == 1);
    }
    CHECK_FALSE(world.Ecs.IsComponentRegistered<TestSpare>());
}

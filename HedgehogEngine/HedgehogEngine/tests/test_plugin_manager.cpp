#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Platform/DynamicLibrary.hpp"
#include "HedgehogEngine/api/Plugins/PluginManager.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "EcsSerialization/api/ComponentTypeRegistry.hpp"
#include "EcsSerialization/api/UnknownComponents.hpp"

#include "ECS/api/ECS.hpp"

#include "FileSystem/tests/test_helpers.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace HedgehogEngine;

namespace
{
    constexpr const char* TEST_PLUGIN      = "HedgehogTestPlugin";
    constexpr const char* COMPONENT_KEY    = "TestPluginComponent";

    // Collects error lines, since engine tests cannot use HedgehogScripting's log capture.
    struct ErrorLog
    {
        std::vector<std::string> Lines;
        int                      Sink = EngineLogger::Logger::Instance().AddSink(
            [this](EngineLogger::LogLevel level, const std::string& message)
            {
                if (level == EngineLogger::LogLevel::Error)
                    Lines.push_back(message);
            });

        ErrorLog() = default;
        ErrorLog(const ErrorLog&)            = delete;
        ErrorLog& operator=(const ErrorLog&) = delete;
        ~ErrorLog() { EngineLogger::Logger::Instance().RemoveSink(Sink); }

        [[nodiscard]] bool HasOne(const std::string& text) const
        {
            return Lines.size() == 1 && Lines[0].find(text) != std::string::npos;
        }
    };

    // The address of a reflected property of the plugin's component on entity, found by name,
    // since the test cannot name the plugin's type.
    template<typename T>
    T* FindProperty(EngineContext& context, ECS::Entity entity, const char* name)
    {
        const EcsSerialization::ComponentInfo* info = context.GetComponentTypes().Find(COMPONENT_KEY);
        if (!info || !info->Has(context.GetECS(), entity))
            return nullptr;
        void* component = info->Get(context.GetECS(), entity);
        for (const Reflection::PropertyDescriptor& property : info->Properties)
        {
            if (std::strcmp(property.name, name) == 0)
                return static_cast<T*>(property.accessor(component));
        }
        return nullptr;
    }

    ECS::Entity AddPluginEntity(EngineContext& context, float value)
    {
        const ECS::Entity entity = context.GetSceneManager().CreateGameObject();
        context.GetComponentTypes().Find(COMPONENT_KEY)->AddDefault(context.GetECS(), entity);
        *FindProperty<float>(context, entity, "Value") = value;
        return entity;
    }

    bool InInspectorOrder(EngineContext& context, const std::string& key)
    {
        const auto infos = context.GetComponentTypes().GetInfosInOrder();
        return std::any_of(infos.begin(), infos.end(), [&key](const auto* info) { return info->Key == key; });
    }

    // Distinct event types, so the bus makes many channels and rehashes its map.
    template<int N>
    struct ManyEvent
    {
    };

    template<int... N>
    void SubscribeMany(EventBus& bus, std::integer_sequence<int, N...>)
    {
        (bus.Subscribe<ManyEvent<N>>([](const ManyEvent<N>&) {}), ...);
    }
}

TEST_CASE("PluginManager - loading registers the plugin's component, system and subscriptions; unloading removes them")
{
    EngineContext  context;
    PluginManager& plugins     = context.GetPlugins();
    const size_t   subscribers = context.GetEventBus().GetTotalSubscriberCount();
    CHECK(plugins.GetDirectory() == GetEngineModuleDirectory());
    CHECK(plugins.GetLoaded().empty());

    REQUIRE(plugins.Load(TEST_PLUGIN));
    CHECK(plugins.IsLoaded(TEST_PLUGIN));
    CHECK(plugins.IsLoaded("hedgehogtestplugin"));
    REQUIRE(plugins.GetLoaded().size() == 1);
    CHECK(plugins.GetLoaded()[0].Name == TEST_PLUGIN);
    CHECK(plugins.GetLoaded()[0].Version == "1.0.0");
    CHECK(plugins.GetLastError(TEST_PLUGIN).empty());
    CHECK(InInspectorOrder(context, COMPONENT_KEY));
    CHECK(context.GetEventBus().GetTotalSubscriberCount() == subscribers + 2);

    // The system runs in its phase through the engine's frame.
    const ECS::Entity entity = AddPluginEntity(context, 2.0f);
    context.UpdateContext(1.0f, 1.0f / 60.0f);
    context.UpdateContext(1.0f, 1.0f / 60.0f);
    CHECK(*FindProperty<int32_t>(context, entity, "Ticks") == 2);

    REQUIRE(plugins.Unload(TEST_PLUGIN));
    CHECK_FALSE(plugins.IsLoaded(TEST_PLUGIN));
    CHECK(plugins.GetLoaded().empty());
    CHECK(context.GetComponentTypes().Find(COMPONENT_KEY) == nullptr);
    CHECK_FALSE(InInspectorOrder(context, COMPONENT_KEY));
    CHECK(context.GetEventBus().GetTotalSubscriberCount() == subscribers);
    CHECK(plugins.GetRetainedLibraryCount() == 0);
    context.UpdateContext(1.0f, 1.0f / 60.0f);
    CHECK_FALSE(plugins.Unload(TEST_PLUGIN));
}

TEST_CASE("PluginManager - a component's values stay in the scene while the plugin is unloaded")
{
    EngineContext  context;
    PluginManager& plugins = context.GetPlugins();
    REQUIRE(plugins.Load(TEST_PLUGIN));
    const ECS::Entity entity = AddPluginEntity(context, 7.5f);

    REQUIRE(plugins.Unload(TEST_PLUGIN));
    CHECK(context.GetECS().HasComponent<EcsSerialization::UnknownComponentsComponent>(entity));
    const std::string saved = context.GetSceneManager().CaptureSnapshot().Yaml;
    CHECK(saved.find(COMPONENT_KEY) != std::string::npos);
    CHECK(saved.find("7.5") != std::string::npos);

    REQUIRE(plugins.Load(TEST_PLUGIN));
    REQUIRE(FindProperty<float>(context, entity, "Value") != nullptr);
    CHECK(*FindProperty<float>(context, entity, "Value") == doctest::Approx(7.5f));
    CHECK_FALSE(context.GetECS().HasComponent<EcsSerialization::UnknownComponentsComponent>(entity));
}

TEST_CASE("PluginManager - loading and unloading work only in Edit mode, and a plugin loads once")
{
    EngineContext  context;
    PluginManager& plugins = context.GetPlugins();

    REQUIRE(context.Play());
    {
        ErrorLog errors;
        CHECK_FALSE(plugins.Load(TEST_PLUGIN));
        CHECK(errors.HasOne("Edit mode"));
    }
    CHECK_FALSE(plugins.IsLoaded(TEST_PLUGIN));
    CHECK(context.GetComponentTypes().Find(COMPONENT_KEY) == nullptr);
    REQUIRE(context.Stop());

    REQUIRE(plugins.Load(TEST_PLUGIN));
    {
        ErrorLog errors;
        CHECK_FALSE(plugins.Load("HEDGEHOGTESTPLUGIN"));
        CHECK(errors.HasOne("already loaded"));
    }
    CHECK(plugins.GetLoaded().size() == 1);
    CHECK(plugins.GetLastError(TEST_PLUGIN).empty());

    REQUIRE(context.Play());
    REQUIRE(context.Pause());
    {
        ErrorLog errors;
        CHECK_FALSE(plugins.Unload(TEST_PLUGIN));
        CHECK(errors.HasOne("Edit mode"));
    }
    CHECK(plugins.IsLoaded(TEST_PLUGIN));
    CHECK(context.GetComponentTypes().Find(COMPONENT_KEY) != nullptr);
    REQUIRE(context.Stop());
    CHECK(plugins.Unload(TEST_PLUGIN));
}

TEST_CASE("PluginManager - an old API, a missing DLL, a DLL without the entry and a name mismatch are refused")
{
    EngineContext  context;
    PluginManager& plugins = context.GetPlugins();

    const auto refused = [&plugins](const std::string& name, const std::string& reason)
    {
        ErrorLog errors;
        CHECK_FALSE(plugins.Load(name));
        CHECK_FALSE(plugins.IsLoaded(name));
        REQUIRE(errors.Lines.size() == 1);
        CHECK(errors.Lines[0].find("[Plugin] " + name + ": ") != std::string::npos);
        CHECK(errors.Lines[0].find(reason) != std::string::npos);
        CHECK(plugins.GetLastError(name).find(reason) != std::string::npos);
    };

    refused("HedgehogTestPluginOldApi", "plugin API version 0");
    refused("NoSuchPlugin", "NoSuchPlugin.dll");
    refused("Logger", "has no symbol HedgehogPluginEntry");
    refused("../Logger", "not a plugin name");

    // The test plugin under another file name names itself otherwise.
    TempDir directory;
    std::filesystem::copy_file(GetEngineModuleDirectory() / "HedgehogTestPlugin.dll", directory.Path() / "Renamed.dll");
    {
        PluginManager elsewhere(context, directory.Path());
        ErrorLog      errors;
        CHECK_FALSE(elsewhere.Load("Renamed"));
        CHECK(errors.HasOne("names the plugin 'HedgehogTestPlugin'"));
    }

    CHECK(plugins.GetLoaded().empty());
    CHECK(context.GetComponentTypes().Find(COMPONENT_KEY) == nullptr);

    // A refusal does not stick: the plugin loads afterwards and its error clears.
    REQUIRE(plugins.Load(TEST_PLUGIN));
    CHECK(plugins.GetLastError(TEST_PLUGIN).empty());
}

TEST_CASE("PluginManager - destroying the context unloads its plugins, also while Playing")
{
    {
        auto context = std::make_unique<EngineContext>();
        REQUIRE(context->GetPlugins().Load(TEST_PLUGIN));
        AddPluginEntity(*context, 3.0f);
        context->UpdateContext(1.0f, 1.0f / 60.0f);
        context.reset();
    }
    {
        auto context = std::make_unique<EngineContext>();
        REQUIRE(context->GetPlugins().Load(TEST_PLUGIN));
        AddPluginEntity(*context, 3.0f);
        REQUIRE(context->Play());
        context->UpdateContext(1.0f, 1.0f / 60.0f);
        context.reset();
    }
    // A fresh context loads it again from scratch.
    EngineContext context;
    CHECK(context.GetPlugins().Load(TEST_PLUGIN));
}

TEST_CASE("PluginManager - an event the plugin created a channel for survives the DLL being unloaded")
{
    EngineContext context;
    REQUIRE(context.GetPlugins().Load(TEST_PLUGIN)); // its system subscribes to an event only it knows
    REQUIRE(context.GetPlugins().Unload(TEST_PLUGIN));

    // The bus grows and rehashes its channels, the plugin's among them, with its DLL gone.
    SubscribeMany(context.GetEventBus(), std::make_integer_sequence<int, 64>{});
    CHECK(context.GetEventBus().GetSubscriberCount<ManyEvent<63>>() == 1);
    context.GetEventBus().Publish(ManyEvent<0>{});
}

TEST_CASE("PluginManager - a plugin that leaves a subscription is reported and its DLL stays loaded")
{
    // The hooks are reached through a second handle to the same module.
    DynamicLibrary hooks;
    REQUIRE_MESSAGE(hooks.Open(GetEngineModuleDirectory() / "HedgehogTestPlugin.dll"), hooks.GetError());
    using LeaveFunction   = void (*)(bool);
    using PublishFunction = void (*)(EventBus*);
    using CountFunction   = int (*)();
    const auto leave   = reinterpret_cast<LeaveFunction>(hooks.FindSymbol("HedgehogTestPluginLeaveSubscription"));
    const auto publish = reinterpret_cast<PublishFunction>(hooks.FindSymbol("HedgehogTestPluginPublishEvent"));
    const auto count   = reinterpret_cast<CountFunction>(hooks.FindSymbol("HedgehogTestPluginGetEventCount"));
    REQUIRE(leave != nullptr);
    REQUIRE(publish != nullptr);
    REQUIRE(count != nullptr);

    {
        EngineContext  context;
        PluginManager& plugins     = context.GetPlugins();
        const size_t   subscribers = context.GetEventBus().GetTotalSubscriberCount();

        leave(true);
        REQUIRE(plugins.Load(TEST_PLUGIN));
        leave(false);
        hooks.Close(); // only the plugin manager holds the DLL now

        {
            ErrorLog errors;
            REQUIRE(plugins.Unload(TEST_PLUGIN));
            CHECK(errors.HasOne("[Plugin] HedgehogTestPlugin: 1 event subscription(s) of its code are still on the event bus"));
        }
        CHECK(plugins.GetRetainedLibraryCount() == 1);
        CHECK_FALSE(plugins.IsLoaded(TEST_PLUGIN));
        CHECK(context.GetEventBus().GetTotalSubscriberCount() == subscribers + 1);

        // The handler left behind still runs, its code still mapped.
        REQUIRE(hooks.Open(GetEngineModuleDirectory() / "HedgehogTestPlugin.dll"));
        const auto countAgain   = reinterpret_cast<CountFunction>(hooks.FindSymbol("HedgehogTestPluginGetEventCount"));
        const auto publishAgain = reinterpret_cast<PublishFunction>(hooks.FindSymbol("HedgehogTestPluginPublishEvent"));
        const int  before       = countAgain();
        publishAgain(&context.GetEventBus());
        CHECK(countAgain() == before + 1);
    }
    hooks.Close();
}

#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Platform/DynamicLibrary.hpp"
#include "HedgehogEngine/api/Plugins/PluginApi.hpp"
#include "HedgehogEngine/api/Plugins/PluginRegistrar.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "EcsSerialization/api/ComponentTypeRegistry.hpp"

#include "ECS/api/ECS.hpp"

#include <filesystem>
#include <memory>
#include <string>

using namespace HedgehogEngine;

namespace
{
    constexpr const char* TEST_PLUGIN = "HedgehogTestPlugin";

    std::filesystem::path PluginPath(const std::string& name)
    {
        return GetEngineModuleDirectory() / (name + ".dll");
    }

    const HedgehogPluginInfo* EntryOf(DynamicLibrary& library)
    {
        const auto entry = reinterpret_cast<PluginEntryFunction>(library.FindSymbol(PLUGIN_ENTRY_NAME));
        REQUIRE(entry != nullptr);
        return entry();
    }
}

TEST_CASE("Plugin library - the engine's folder holds the test plugin, whose info is compatible")
{
    CHECK(std::filesystem::is_regular_file(GetEngineModuleDirectory() / "HedgehogEngine.dll"));

    DynamicLibrary library;
    REQUIRE_MESSAGE(library.Open(PluginPath(TEST_PLUGIN)), library.GetError());
    CHECK(library.IsOpen());
    const HedgehogPluginInfo* info = EntryOf(library);
    REQUIRE(info != nullptr);
    CHECK(std::string(info->Name) == TEST_PLUGIN);
    CHECK(std::string(info->Version) == "1.0.0");
    CHECK(info->Register != nullptr);
    CHECK(info->Unregister == nullptr);
    CHECK(CheckCompatible(*info).empty());

    library.Close();
    CHECK_FALSE(library.IsOpen());
}

TEST_CASE("Plugin library - a plugin registered, undone and unloaded leaves nothing that calls into it")
{
    auto context = std::make_unique<EngineContext>();
    const size_t engineSubscribers = context->GetEventBus().GetTotalSubscriberCount();
    {
        DynamicLibrary library;
        REQUIRE_MESSAGE(library.Open(PluginPath(TEST_PLUGIN)), library.GetError());
        const HedgehogPluginInfo* info = EntryOf(library);

        PluginRegistrar registrar(*context, info->Name);
        REQUIRE(info->Register(registrar));
        CHECK(context->GetComponentTypes().Find("TestPluginComponent") != nullptr);
        // The plugin's system subscribed in its OnRegister, with a handler of the plugin's code.
        CHECK(context->GetEventBus().GetTotalSubscriberCount() == engineSubscribers + 1);

        const ECS::Entity entity = context->GetSceneManager().CreateGameObject();
        context->GetComponentTypes().Find("TestPluginComponent")->AddDefault(context->GetECS(), entity);
        context->UpdateContext(1.0f, 1.0f / 60.0f);
        context->GetEventBus().Publish(TransformChangedEvent{ entity });

        registrar.UnregisterAll();
        CHECK(context->GetEventBus().GetTotalSubscriberCount() == engineSubscribers);
        library.Close();
    }

    // The DLL is gone: publishing, a frame and tearing everything down must not reach its code.
    context->GetEventBus().Publish(TransformChangedEvent{ 0 });
    context->UpdateContext(1.0f, 1.0f / 60.0f);
    CHECK(context->GetComponentTypes().Find("TestPluginComponent") == nullptr);
    context.reset();
}

TEST_CASE("Plugin library - a missing file and a library without the entry are reported")
{
    DynamicLibrary missing;
    CHECK_FALSE(missing.Open(PluginPath("NoSuchPlugin")));
    CHECK(missing.GetError().find("NoSuchPlugin.dll") != std::string::npos);
    CHECK_FALSE(missing.IsOpen());

    DynamicLibrary logger;
    REQUIRE_MESSAGE(logger.Open(PluginPath("Logger")), logger.GetError());
    CHECK(logger.FindSymbol(PLUGIN_ENTRY_NAME) == nullptr);
    CHECK(logger.GetError() == "Logger.dll has no symbol HedgehogPluginEntry");
}

TEST_CASE("Plugin library - another API version, compiler or build configuration is refused")
{
    const HedgehogPluginInfo engine = MakeEngineInfo();
    CHECK(engine.ApiVersion == HH_PLUGIN_API_VERSION);
    CHECK(CheckCompatible(engine).empty());

    HedgehogPluginInfo info = engine;
    info.ApiVersion         = HH_PLUGIN_API_VERSION + 1;
    CHECK(CheckCompatible(info) == "it was built with plugin API version " + std::to_string(HH_PLUGIN_API_VERSION + 1) +
                                       ", but the engine with " + std::to_string(HH_PLUGIN_API_VERSION));

    info                 = engine;
    info.CompilerVersion = 1900;
    const std::string compiler = CheckCompatible(info);
    CHECK(compiler.find("compiler version 1900") != std::string::npos);
    CHECK(compiler.find(std::to_string(engine.CompilerVersion)) != std::string::npos);

    info            = engine;
    info.DebugBuild = engine.DebugBuild ? 0 : 1;
    const std::string configuration = CheckCompatible(info);
    CHECK(configuration.find("Debug") != std::string::npos);
    CHECK(configuration.find("Release") != std::string::npos);
}

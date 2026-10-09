#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include <cstdint>
#include <string>

// The version of everything a plugin touches: PluginRegistrar, EngineContext and the engine, ECS
// and EcsSerialization types a plugin uses. Bump it whenever one of them changes layout or
// meaning, so plugins built against the old one are refused instead of crashing.
// 2 (HE-310): ECS::System gained OnPostFixedUpdate. 3 (HE-312): EngineContext gained the physics
// world and system.
#define HH_PLUGIN_API_VERSION 4u

namespace HedgehogEngine
{
    class PluginRegistrar;

    // What a plugin DLL describes itself with: the build it was made by, its name (the stem of its
    // DLL), its version, and what it registers. C++ crosses the boundary, so the engine refuses a
    // plugin unless ApiVersion, CompilerVersion and DebugBuild all match its own (CheckCompatible).
    struct HedgehogPluginInfo
    {
        uint32_t    ApiVersion      = 0;
        uint32_t    CompilerVersion = 0; // _MSC_VER
        uint32_t    DebugBuild      = 0; // 1 for a Debug (debug runtime) build
        const char* Name            = nullptr;
        const char* Version         = nullptr;
        // Registers everything through the registrar; false fails the load, and what it
        // registered is undone.
        bool (*Register)(PluginRegistrar& registrar) = nullptr;
        // Optional: runs before the registrar undoes the registrations (release what the plugin
        // holds outside them).
        void (*Unregister)(PluginRegistrar& registrar) = nullptr;
    };

    // The symbol every plugin DLL exports, of type PluginEntryFunction.
    inline constexpr const char* PLUGIN_ENTRY_NAME = "HedgehogPluginEntry";
    using PluginEntryFunction = const HedgehogPluginInfo* (*)();

    // The values of the module that compiles this, which HH_PLUGIN writes into a plugin's info.
    constexpr uint32_t GetCompilerVersion()
    {
#if defined(_MSC_VER)
        return _MSC_VER;
#else
        return 0;
#endif
    }

    constexpr uint32_t GetDebugBuild()
    {
#if defined(_DEBUG)
        return 1;
#else
        return 0;
#endif
    }

    // The engine's own values (compiled into HedgehogEngine.dll), with no name or callbacks.
    [[nodiscard]] HEDGEHOG_ENGINE_API HedgehogPluginInfo MakeEngineInfo();

    // Why the engine cannot load a plugin with this info (another API version, compiler version or
    // build configuration, naming both values), or empty when it can.
    [[nodiscard]] HEDGEHOG_ENGINE_API std::string CheckCompatible(const HedgehogPluginInfo& info);
}

// Defines a plugin's entry point. In one .cpp of the plugin:
//   HH_PLUGIN("Spinner", "1.0.0", &RegisterSpinner, nullptr)
// name must be the plugin DLL's file name without ".dll".
#define HH_PLUGIN(NAME, VERSION, REGISTER, UNREGISTER)                                                          \
    extern "C" __declspec(dllexport) const ::HedgehogEngine::HedgehogPluginInfo* HedgehogPluginEntry()         \
    {                                                                                                          \
        static const ::HedgehogEngine::HedgehogPluginInfo info{ HH_PLUGIN_API_VERSION,                         \
                                                                ::HedgehogEngine::GetCompilerVersion(),        \
                                                                ::HedgehogEngine::GetDebugBuild(),             \
                                                                NAME,                                          \
                                                                VERSION,                                       \
                                                                REGISTER,                                      \
                                                                UNREGISTER };                                  \
        return &info;                                                                                          \
    }

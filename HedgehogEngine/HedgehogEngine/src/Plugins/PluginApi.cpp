#include "HedgehogEngine/api/Plugins/PluginApi.hpp"

namespace HedgehogEngine
{
    namespace
    {
        std::string Mismatch(const char* what, uint32_t plugin, uint32_t engine)
        {
            return "it was built with " + std::string(what) + " " + std::to_string(plugin) + ", but the engine with " +
                   std::to_string(engine);
        }
    }

    HedgehogPluginInfo MakeEngineInfo()
    {
        HedgehogPluginInfo info;
        info.ApiVersion      = HH_PLUGIN_API_VERSION;
        info.CompilerVersion = GetCompilerVersion();
        info.DebugBuild      = GetDebugBuild();
        return info;
    }

    std::string CheckCompatible(const HedgehogPluginInfo& info)
    {
        const HedgehogPluginInfo engine = MakeEngineInfo();
        if (info.ApiVersion != engine.ApiVersion)
            return Mismatch("plugin API version", info.ApiVersion, engine.ApiVersion);
        if (info.CompilerVersion != engine.CompilerVersion)
            return Mismatch("compiler version", info.CompilerVersion, engine.CompilerVersion);
        if (info.DebugBuild != engine.DebugBuild)
        {
            const auto configuration = [](uint32_t debug) { return std::string(debug ? "Debug" : "Release"); };
            return "it is a " + configuration(info.DebugBuild) + " build, but the engine is a " +
                   configuration(engine.DebugBuild) + " build";
        }
        return {};
    }
}

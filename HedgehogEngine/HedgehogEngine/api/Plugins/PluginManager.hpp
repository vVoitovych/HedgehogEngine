#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogEngine/api/Platform/DynamicLibrary.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace HedgehogSettings
{
    class ProjectSettings;
}

namespace HedgehogEngine
{
    class EngineContext;
    class PluginRegistrar;
    struct HedgehogPluginInfo;

    struct LoadedPlugin
    {
        std::string Name;
        std::string Version;
    };

    // Loads plugin DLLs into an EngineContext and unloads them again (EngineContext::GetPlugins).
    // A plugin is <directory>/<name>.dll, by default beside HedgehogEngine.dll. Names compare
    // ignoring case, as the project's plugin list does.
    //
    // Load and Unload work in Edit mode only, since the Play snapshot and running systems must not
    // see types come and go. Unloading undoes everything the plugin registered (its components'
    // values stay in the scene, kept as unknown components, and come back when it loads again),
    // then closes the DLL, unless an event subscription made by the plugin's code is still on the
    // engine's bus: the DLL then stays loaded until the process ends, with an error, so the
    // handler never runs unmapped code.
    class PluginManager
    {
    public:
        HEDGEHOG_ENGINE_API explicit PluginManager(EngineContext& engine,
                                                   std::filesystem::path directory = GetEngineModuleDirectory());
        HEDGEHOG_ENGINE_API ~PluginManager();

        PluginManager(const PluginManager&)            = delete;
        PluginManager& operator=(const PluginManager&) = delete;

        // Opens <name>.dll and registers the plugin. False, with one "[Plugin] <name>: <why>." error
        // (GetLastError) and nothing changed, outside Edit mode, for an invalid or loaded name, a
        // DLL that does not open, has no entry, is built for another engine (CheckCompatible) or
        // names another plugin, and when its Register fails (what it registered is undone).
        HEDGEHOG_ENGINE_API bool Load(const std::string& name);
        // Unregisters the plugin and closes its DLL. False outside Edit mode or when not loaded.
        HEDGEHOG_ENGINE_API bool Unload(const std::string& name);
        // Unloads every plugin, last loaded first, whatever the play state (the context's
        // destructor).
        HEDGEHOG_ENGINE_API void UnloadAll();

        // Makes the loaded plugins the project's enabled ones: unloads, last loaded first, those no
        // longer listed or disabled, then loads, in list order, the enabled ones not loaded. A
        // plugin that fails is logged and the rest go on (a scene's data of a missing plugin stays
        // kept as unknown components). Returns how many failed.
        HEDGEHOG_ENGINE_API size_t ApplyProjectPlugins(const HedgehogSettings::ProjectSettings& project);

        [[nodiscard]] HEDGEHOG_ENGINE_API std::vector<LoadedPlugin> GetLoaded() const;
        [[nodiscard]] HEDGEHOG_ENGINE_API bool IsLoaded(const std::string& name) const;
        // Why the last Load of name failed, or empty once it loaded.
        [[nodiscard]] HEDGEHOG_ENGINE_API std::string GetLastError(const std::string& name) const;
        [[nodiscard]] HEDGEHOG_ENGINE_API const std::filesystem::path& GetDirectory() const;
        // How many unloaded plugins' DLLs stay loaded because they left event subscriptions.
        [[nodiscard]] HEDGEHOG_ENGINE_API size_t GetRetainedLibraryCount() const;

    private:
        struct Plugin
        {
            std::string                      Name; // as the DLL names itself
            std::string                      Version;
            const HedgehogPluginInfo*        Info = nullptr;
            DynamicLibrary                   Library;
            std::unique_ptr<PluginRegistrar> Registrar;
        };

        [[nodiscard]] Plugin* Find(const std::string& name);
        [[nodiscard]] const Plugin* Find(const std::string& name) const;
        bool Fail(const std::string& name, const std::string& why);
        void UnloadPlugin(Plugin& plugin);

        EngineContext&                               m_Engine;
        std::filesystem::path                        m_Directory;
        std::vector<std::unique_ptr<Plugin>>         m_Plugins; // load order
        std::unordered_map<std::string, std::string> m_Errors;  // by lower-case name
        size_t                                       m_RetainedLibraries = 0;
    };
}

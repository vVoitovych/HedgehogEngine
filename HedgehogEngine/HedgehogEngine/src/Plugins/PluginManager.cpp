#include "HedgehogEngine/api/Plugins/PluginManager.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Plugins/PluginApi.hpp"
#include "HedgehogEngine/api/Plugins/PluginRegistrar.hpp"

#include "HedgehogSettings/api/ProjectSettings.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <cctype>
#include <typeinfo>

namespace HedgehogEngine
{
    namespace
    {
        std::string ToLower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        bool SameName(const std::string& a, const std::string& b)
        {
            return ToLower(a) == ToLower(b);
        }
    }

    PluginManager::PluginManager(EngineContext& engine, std::filesystem::path directory)
        : m_Engine(engine)
        , m_Directory(std::move(directory))
    {
    }

    PluginManager::~PluginManager()
    {
        UnloadAll();
    }

    bool PluginManager::Load(const std::string& name)
    {
        // Refusals that say nothing about the plugin itself are logged without becoming its error.
        if (m_Engine.GetPlayState() != PlayState::Edit)
        {
            LOGERROR("[Plugin] " + name + ": plugins load only in Edit mode.");
            return false;
        }
        if (Find(name))
        {
            LOGERROR("[Plugin] " + name + ": it is already loaded.");
            return false;
        }
        if (!HedgehogSettings::ProjectSettings::IsValidPluginName(name))
            return Fail(name, "it is not a plugin name (1 to 64 of A-Z, a-z, 0-9, _ and -)");

        auto plugin = std::make_unique<Plugin>();
        if (!plugin->Library.Open(m_Directory / (name + ".dll")))
            return Fail(name, plugin->Library.GetError());
        const auto entry = reinterpret_cast<PluginEntryFunction>(plugin->Library.FindSymbol(PLUGIN_ENTRY_NAME));
        if (!entry)
            return Fail(name, plugin->Library.GetError());

        const HedgehogPluginInfo* info = entry();
        if (!info)
            return Fail(name, "its entry gives no plugin info");
        if (const std::string why = CheckCompatible(*info); !why.empty())
            return Fail(name, why);
        if (!info->Name || !SameName(info->Name, name))
            return Fail(name, "its DLL names the plugin '" + std::string(info->Name ? info->Name : "") + "'");
        if (!info->Register)
            return Fail(name, "it has no Register function");

        plugin->Name      = info->Name;
        plugin->Version   = info->Version ? info->Version : "";
        plugin->Info      = info;
        plugin->Registrar = std::make_unique<PluginRegistrar>(m_Engine, plugin->Name);

        bool registered = false;
        try
        {
            registered = info->Register(*plugin->Registrar);
        }
        catch (const std::exception& exception)
        {
            LOGERROR("[Plugin] " + plugin->Name + ": Register threw: " + exception.what());
        }
        catch (...)
        {
            LOGERROR("[Plugin] " + plugin->Name + ": Register threw.");
        }
        if (!registered)
        {
            // Undone while the DLL is loaded, since the undo steps are its code.
            plugin->Registrar->UnregisterAll();
            plugin->Registrar.reset();
            return Fail(name, "its Register failed; what it registered is undone");
        }

        m_Errors.erase(ToLower(name));
        LOGINFO("[Plugin] Loaded " + plugin->Name + " " + plugin->Version + ".");
        m_Plugins.push_back(std::move(plugin));
        return true;
    }

    bool PluginManager::Unload(const std::string& name)
    {
        if (m_Engine.GetPlayState() != PlayState::Edit)
        {
            LOGERROR("[Plugin] " + name + ": plugins unload only in Edit mode.");
            return false;
        }
        const auto found = std::find_if(m_Plugins.begin(), m_Plugins.end(),
                                        [&name](const std::unique_ptr<Plugin>& plugin) { return SameName(plugin->Name, name); });
        if (found == m_Plugins.end())
        {
            LOGERROR("[Plugin] " + name + ": it is not loaded.");
            return false;
        }
        std::unique_ptr<Plugin> plugin = std::move(*found);
        m_Plugins.erase(found);
        UnloadPlugin(*plugin);
        return true;
    }

    void PluginManager::UnloadAll()
    {
        while (!m_Plugins.empty())
        {
            std::unique_ptr<Plugin> plugin = std::move(m_Plugins.back());
            m_Plugins.pop_back();
            UnloadPlugin(*plugin);
        }
    }

    size_t PluginManager::ApplyProjectPlugins(const HedgehogSettings::ProjectSettings& project)
    {
        const std::vector<HedgehogSettings::PluginEntry>& entries = project.GetPlugins();
        const auto isEnabled = [&entries](const std::string& name)
        {
            return std::any_of(entries.begin(), entries.end(), [&name](const HedgehogSettings::PluginEntry& entry)
                               { return entry.Enabled && SameName(entry.Name, name); });
        };

        size_t failed = 0;
        std::vector<std::string> unwanted;
        for (auto plugin = m_Plugins.rbegin(); plugin != m_Plugins.rend(); ++plugin)
        {
            if (!isEnabled((*plugin)->Name))
                unwanted.push_back((*plugin)->Name);
        }
        for (const std::string& name : unwanted)
            failed += Unload(name) ? 0 : 1;

        for (const HedgehogSettings::PluginEntry& entry : entries)
        {
            if (entry.Enabled && !IsLoaded(entry.Name))
                failed += Load(entry.Name) ? 0 : 1;
        }
        return failed;
    }

    void PluginManager::UnloadPlugin(Plugin& plugin)
    {
        if (plugin.Info->Unregister)
            plugin.Info->Unregister(*plugin.Registrar);
        plugin.Registrar->UnregisterAll();
        plugin.Registrar.reset();

        // A handler the plugin subscribed without the registrar and never removed would run its
        // code: keep the DLL loaded rather than leave that code unmapped.
        DynamicLibrary& library = plugin.Library;
        const size_t    left    = m_Engine.GetEventBus().CountSubscribersWhere(
            [&library](const std::type_info& handlerType) { return library.Contains(&handlerType); });
        if (left > 0)
        {
            LOGERROR("[Plugin] " + plugin.Name + ": " + std::to_string(left) +
                     " event subscription(s) of its code are still on the event bus; its DLL stays loaded.");
            library.Release();
            ++m_RetainedLibraries;
        }
        else
        {
            library.Close();
        }
        LOGINFO("[Plugin] Unloaded " + plugin.Name + ".");
    }

    std::vector<LoadedPlugin> PluginManager::GetLoaded() const
    {
        std::vector<LoadedPlugin> loaded;
        loaded.reserve(m_Plugins.size());
        for (const std::unique_ptr<Plugin>& plugin : m_Plugins)
            loaded.push_back(LoadedPlugin{ plugin->Name, plugin->Version });
        return loaded;
    }

    bool PluginManager::IsLoaded(const std::string& name) const { return Find(name) != nullptr; }

    std::string PluginManager::GetLastError(const std::string& name) const
    {
        const auto found = m_Errors.find(ToLower(name));
        return found == m_Errors.end() ? std::string() : found->second;
    }

    const std::filesystem::path& PluginManager::GetDirectory() const { return m_Directory; }
    size_t PluginManager::GetRetainedLibraryCount() const { return m_RetainedLibraries; }

    PluginManager::Plugin* PluginManager::Find(const std::string& name)
    {
        for (const std::unique_ptr<Plugin>& plugin : m_Plugins)
        {
            if (SameName(plugin->Name, name))
                return plugin.get();
        }
        return nullptr;
    }

    const PluginManager::Plugin* PluginManager::Find(const std::string& name) const
    {
        for (const std::unique_ptr<Plugin>& plugin : m_Plugins)
        {
            if (SameName(plugin->Name, name))
                return plugin.get();
        }
        return nullptr;
    }

    bool PluginManager::Fail(const std::string& name, const std::string& why)
    {
        LOGERROR("[Plugin] " + name + ": " + why + ".");
        m_Errors[ToLower(name)] = why;
        return false;
    }
}

#include "HedgehogEngine/api/Plugins/PluginManager.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Plugins/PluginApi.hpp"
#include "HedgehogEngine/api/Plugins/PluginRegistrar.hpp"

#include "HedgehogSettings/api/ProjectSettings.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <string>
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

        // Numbers the managers of one process, so each has its own shadow folder.
        std::atomic<uint32_t> s_ManagerCount{ 0 };

        std::filesystem::path MakeShadowDirectory()
        {
            std::error_code       error;
            std::filesystem::path temp = std::filesystem::temp_directory_path(error);
            if (error)
                temp = std::filesystem::current_path(error);
            return temp / "HedgehogPlugins" /
                   (std::to_string(GetCurrentProcessNumber()) + "-" + std::to_string(++s_ManagerCount));
        }
    }

    PluginManager::ShadowFile::~ShadowFile()
    {
        std::error_code error;
        if (!Path.empty())
            std::filesystem::remove(Path, error); // fails, harmlessly, for a DLL kept loaded
    }

    PluginManager::PluginManager(EngineContext& engine, std::filesystem::path directory)
        : m_Engine(engine)
        , m_Directory(std::move(directory))
        , m_ShadowDirectory(MakeShadowDirectory())
    {
    }

    PluginManager::~PluginManager()
    {
        UnloadAll();
        std::error_code error;
        std::filesystem::remove_all(m_ShadowDirectory, error);
    }

    void PluginManager::SetShadowCopy(bool enabled) { m_ShadowCopy = enabled; }
    bool PluginManager::IsShadowCopy() const { return m_ShadowCopy; }
    const std::filesystem::path& PluginManager::GetShadowDirectory() const { return m_ShadowDirectory; }

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

        auto plugin    = std::make_unique<Plugin>();
        plugin->Source = m_Directory / (name + ".dll");
        std::filesystem::path opened = plugin->Source;
        std::error_code       error;
        // A copy is opened, so the plugin's own DLL stays free to be rebuilt. A missing DLL is
        // opened as it is, for the usual error.
        if (m_ShadowCopy && std::filesystem::is_regular_file(plugin->Source, error))
        {
            opened = m_ShadowDirectory / (name + "-" + std::to_string(++m_ShadowCount) + ".dll");
            std::filesystem::create_directories(m_ShadowDirectory, error);
            if (!std::filesystem::copy_file(plugin->Source, opened, std::filesystem::copy_options::overwrite_existing, error))
                return Fail(name, "cannot copy " + plugin->Source.string() + " to " + opened.string() + ": " + error.message());
            plugin->Shadow.Path = opened;
        }
        plugin->WriteTime = std::filesystem::last_write_time(plugin->Source, error);
        if (!plugin->Library.Open(opened))
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
        std::erase_if(m_FailedReloads, [&name](const FailedReload& failed) { return SameName(failed.Name, name); });
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

    bool PluginManager::Reload(const std::string& name)
    {
        if (m_Engine.GetPlayState() != PlayState::Edit)
        {
            LOGERROR("[Plugin] " + name + ": plugins reload only in Edit mode.");
            return false;
        }
        const Plugin* plugin = Find(name);
        if (!plugin)
        {
            LOGERROR("[Plugin] " + name + ": it is not loaded.");
            return false;
        }
        const std::string           pluginName = plugin->Name;
        const std::filesystem::path source     = plugin->Source;

        (void)Unload(pluginName);
        if (Load(pluginName))
            return true;
        // Watched, so a fixed DLL loads once it is rebuilt again.
        std::error_code error;
        m_FailedReloads.push_back(FailedReload{ pluginName, source, std::filesystem::last_write_time(source, error) });
        return false;
    }

    size_t PluginManager::ReloadChangedPlugins(std::chrono::steady_clock::time_point now)
    {
        if (m_Polled && now - m_LastPoll < RELOAD_POLL_INTERVAL)
            return 0;
        m_Polled   = true;
        m_LastPoll = now;
        if (m_Engine.GetPlayState() != PlayState::Edit)
            return 0;

        std::error_code          error;
        std::vector<std::string> changed;
        for (const std::unique_ptr<Plugin>& plugin : m_Plugins)
        {
            const auto writeTime = std::filesystem::last_write_time(plugin->Source, error);
            if (!error && writeTime != plugin->WriteTime)
                changed.push_back(plugin->Name);
        }

        size_t reloaded = 0;
        for (const std::string& name : changed)
        {
            if (Reload(name))
            {
                LOGINFO("[Plugin] Reloaded " + name + ".");
                ++reloaded;
            }
        }

        // Failed reloads whose DLL has been rebuilt since; Load drops an entry when it succeeds.
        const std::vector<FailedReload> failed = m_FailedReloads;
        for (const FailedReload& entry : failed)
        {
            const auto writeTime = std::filesystem::last_write_time(entry.Source, error);
            if (error || writeTime == entry.WriteTime || IsLoaded(entry.Name))
                continue;
            if (Load(entry.Name))
            {
                LOGINFO("[Plugin] Reloaded " + entry.Name + ".");
                ++reloaded;
            }
            else
            {
                for (FailedReload& watched : m_FailedReloads)
                    if (SameName(watched.Name, entry.Name))
                        watched.WriteTime = writeTime; // tried; the next rebuild is
            }
        }
        return reloaded;
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

        // A plugin no longer wanted is not loaded again when its DLL is rebuilt.
        std::erase_if(m_FailedReloads, [&isEnabled](const FailedReload& entry) { return !isEnabled(entry.Name); });

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

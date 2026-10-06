#include "HedgehogSettings/api/ProjectSettings.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "Logger/api/Logger.hpp"

#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <cctype>

namespace HedgehogSettings
{
    namespace
    {
        constexpr std::string_view ASSETS_PREFIX = "assets://";
        constexpr std::string_view SCENE_SUFFIX  = ".yaml";

        std::string WithSlashes(std::string path)
        {
            std::replace(path.begin(), path.end(), '\\', '/');
            return path;
        }

        bool EqualIgnoringCase(std::string_view a, std::string_view b)
        {
            return a.size() == b.size() &&
                   std::equal(a.begin(), a.end(), b.begin(),
                              [](char x, char y) { return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y)); });
        }

        std::vector<PluginEntry>::iterator FindPlugin(std::vector<PluginEntry>& plugins, std::string_view name)
        {
            return std::find_if(plugins.begin(), plugins.end(), [name](const PluginEntry& entry) { return EqualIgnoringCase(entry.Name, name); });
        }

        // Why name cannot join plugins, or empty.
        std::string CheckNewPlugin(const std::vector<PluginEntry>& plugins, const std::string& name)
        {
            if (!ProjectSettings::IsValidPluginName(name))
                return "'" + name + "' is not a plugin name (1 to 64 of A-Z, a-z, 0-9, '_' and '-')";
            const bool listed = std::any_of(plugins.begin(), plugins.end(), [&name](const PluginEntry& entry) { return EqualIgnoringCase(entry.Name, name); });
            if (listed)
                return "the plugin '" + name + "' is already listed";
            return {};
        }

        template<typename T>
        void Assign(T& field, const T& value, bool& dirty)
        {
            if (field == value)
                return;
            field = value;
            dirty = true;
        }
    }

    ProjectSettings::ProjectSettings() = default;

    bool ProjectSettings::IsValidName(std::string_view name)
    {
        if (name.empty() || name.size() > MAX_NAME_LENGTH || name.front() == ' ' || name.back() == ' ')
            return false;
        return std::all_of(name.begin(), name.end(),
                           [](char c)
                           {
                               return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                                      c == '_' || c == '-' || c == ' ';
                           });
    }

    bool ProjectSettings::IsValidStartupScene(std::string_view virtualPath)
    {
        if (virtualPath.empty())
            return true;
        const std::string path = WithSlashes(std::string(virtualPath));
        if (!path.starts_with(ASSETS_PREFIX) || !path.ends_with(SCENE_SUFFIX) ||
            path.size() <= ASSETS_PREFIX.size() + SCENE_SUFFIX.size())
            return false;

        // No ".." segment and no empty one: the path stays inside assets://.
        const std::string_view relative = std::string_view(path).substr(ASSETS_PREFIX.size());
        size_t                 start    = 0;
        while (start <= relative.size())
        {
            const size_t           end     = std::min(relative.find('/', start), relative.size());
            const std::string_view segment = relative.substr(start, end - start);
            if (segment.empty() || segment == "." || segment == ".." || segment.find(':') != std::string_view::npos)
                return false;
            start = end + 1;
        }
        return true;
    }

    bool ProjectSettings::IsValidPluginName(std::string_view name)
    {
        if (name.empty() || name.size() > MAX_PLUGIN_NAME_LENGTH)
            return false;
        return std::all_of(name.begin(), name.end(),
                           [](char c)
                           {
                               return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
                                      c == '-';
                           });
    }

    const std::string& ProjectSettings::GetName() const { return m_Name; }

    bool ProjectSettings::SetName(const std::string& name)
    {
        if (!IsValidName(name))
        {
            LOGWARNING("[Project] '" + name + "' is not a project name (1 to 64 of A-Z, a-z, 0-9, '_', '-' and inner "
                       "spaces); keeping '" + m_Name + "'.");
            return false;
        }
        Assign(m_Name, name, m_IsDirty);
        return true;
    }

    const std::string& ProjectSettings::GetStartupScene() const { return m_StartupScene; }

    bool ProjectSettings::SetStartupScene(const std::string& virtualPath)
    {
        if (!IsValidStartupScene(virtualPath))
        {
            LOGWARNING("[Project] '" + virtualPath + "' is not a startup scene: it must be a .yaml file under assets://.");
            return false;
        }
        Assign(m_StartupScene, WithSlashes(virtualPath), m_IsDirty);
        return true;
    }

    const std::string& ProjectSettings::GetWindowTitle() const { return m_WindowTitle; }

    void ProjectSettings::SetWindowTitle(const std::string& title) { Assign(m_WindowTitle, title, m_IsDirty); }

    uint32_t ProjectSettings::GetWindowWidth() const { return m_WindowWidth; }

    uint32_t ProjectSettings::GetWindowHeight() const { return m_WindowHeight; }

    void ProjectSettings::SetWindowSize(uint32_t width, uint32_t height)
    {
        Assign(m_WindowWidth, std::clamp(width, MIN_WINDOW_SIZE, MAX_WINDOW_SIZE), m_IsDirty);
        Assign(m_WindowHeight, std::clamp(height, MIN_WINDOW_SIZE, MAX_WINDOW_SIZE), m_IsDirty);
    }

    bool ProjectSettings::IsFullscreen() const { return m_Fullscreen; }

    void ProjectSettings::SetFullscreen(bool fullscreen) { Assign(m_Fullscreen, fullscreen, m_IsDirty); }

    bool ProjectSettings::IsVSync() const { return m_VSync; }

    void ProjectSettings::SetVSync(bool vsync) { Assign(m_VSync, vsync, m_IsDirty); }

    int ProjectSettings::GetGameDataVersion() const { return m_GameDataVersion; }

    void ProjectSettings::SetGameDataVersion(int version) { Assign(m_GameDataVersion, std::max(version, 1), m_IsDirty); }

    const std::vector<PluginEntry>& ProjectSettings::GetPlugins() const { return m_Plugins; }

    bool ProjectSettings::AddPlugin(const std::string& name, bool enabled)
    {
        if (const std::string why = CheckNewPlugin(m_Plugins, name); !why.empty())
        {
            LOGWARNING("[Project] " + why + "; it is not added.");
            return false;
        }
        m_Plugins.push_back(PluginEntry{ name, enabled });
        m_IsDirty = true;
        return true;
    }

    bool ProjectSettings::RemovePlugin(const std::string& name)
    {
        const auto found = FindPlugin(m_Plugins, name);
        if (found == m_Plugins.end())
        {
            LOGWARNING("[Project] The plugin '" + name + "' is not listed; nothing is removed.");
            return false;
        }
        m_Plugins.erase(found);
        m_IsDirty = true;
        return true;
    }

    bool ProjectSettings::SetPluginEnabled(const std::string& name, bool enabled)
    {
        const auto found = FindPlugin(m_Plugins, name);
        if (found == m_Plugins.end())
        {
            LOGWARNING("[Project] The plugin '" + name + "' is not listed; it cannot be enabled or disabled.");
            return false;
        }
        Assign(found->Enabled, enabled, m_IsDirty);
        return true;
    }

    bool ProjectSettings::SetPlugins(const std::vector<PluginEntry>& plugins)
    {
        std::vector<PluginEntry> accepted;
        for (const PluginEntry& entry : plugins)
        {
            if (const std::string why = CheckNewPlugin(accepted, entry.Name); !why.empty())
            {
                LOGWARNING("[Project] " + why + "; the plugin list is not changed.");
                return false;
            }
            accepted.push_back(entry);
        }
        Assign(m_Plugins, accepted, m_IsDirty);
        return true;
    }

    bool ProjectSettings::Load(const std::string& virtualPath, const FS::FileSystemManager& fileSystem)
    {
        if (!fileSystem.Exists(virtualPath))
            return false;
        const auto text = fileSystem.ReadTextFile(virtualPath);
        if (!text)
            return false;

        ProjectSettings loaded;
        try
        {
            const YAML::Node root = YAML::Load(*text);
            if (const YAML::Node n = root["name"])
                (void)loaded.SetName(n.as<std::string>());
            if (const YAML::Node n = root["startup_scene"])
                (void)loaded.SetStartupScene(n.as<std::string>());
            if (const YAML::Node window = root["window"])
            {
                if (const YAML::Node n = window["title"])
                    loaded.SetWindowTitle(n.as<std::string>());
                const YAML::Node width  = window["width"];
                const YAML::Node height = window["height"];
                loaded.SetWindowSize(width ? width.as<uint32_t>() : DEFAULT_WINDOW_WIDTH,
                                     height ? height.as<uint32_t>() : DEFAULT_WINDOW_HEIGHT);
                if (const YAML::Node n = window["fullscreen"])
                    loaded.SetFullscreen(n.as<bool>());
                if (const YAML::Node n = window["vsync"])
                    loaded.SetVSync(n.as<bool>());
            }
            if (const YAML::Node n = root["game_data_version"])
                loaded.SetGameDataVersion(n.as<int>());
            if (const YAML::Node plugins = root["plugins"])
            {
                if (!plugins.IsSequence())
                    LOGWARNING("[Project] " + virtualPath + ": plugins is not a list; no plugin is loaded.");
                for (size_t index = 0; plugins.IsSequence() && index < plugins.size(); ++index)
                {
                    const YAML::Node entry    = plugins[index];
                    bool             enabled  = true;
                    const bool       readable = entry.IsMap() && entry["name"] && entry["name"].IsScalar() &&
                                          (!entry["enabled"] || YAML::convert<bool>::decode(entry["enabled"], enabled));
                    if (!readable)
                    {
                        LOGWARNING("[Project] " + virtualPath + ": plugins entry " + std::to_string(index) +
                                   " is not { name, enabled }; it is skipped.");
                        continue;
                    }
                    (void)loaded.AddPlugin(entry["name"].as<std::string>(), enabled);
                }
            }
        }
        catch (const YAML::Exception& e)
        {
            LOGERROR("[Project] " + virtualPath + " is malformed, keeping the project settings as they were: " + e.what());
            return false;
        }

        *this = loaded;
        CleanDirtyState();
        return true;
    }

    bool ProjectSettings::Save(const std::string& virtualPath, const FS::FileSystemManager& fileSystem)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "name" << YAML::Value << m_Name;
        out << YAML::Key << "startup_scene" << YAML::Value << m_StartupScene;
        out << YAML::Key << "window" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "title" << YAML::Value << m_WindowTitle;
        out << YAML::Key << "width" << YAML::Value << m_WindowWidth;
        out << YAML::Key << "height" << YAML::Value << m_WindowHeight;
        out << YAML::Key << "fullscreen" << YAML::Value << m_Fullscreen;
        out << YAML::Key << "vsync" << YAML::Value << m_VSync;
        out << YAML::EndMap;
        out << YAML::Key << "game_data_version" << YAML::Value << m_GameDataVersion;
        out << YAML::Key << "plugins" << YAML::Value;
        if (m_Plugins.empty())
            out << YAML::Flow;
        out << YAML::BeginSeq;
        for (const PluginEntry& plugin : m_Plugins)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "name" << YAML::Value << plugin.Name;
            out << YAML::Key << "enabled" << YAML::Value << plugin.Enabled;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;
        out << YAML::EndMap;

        if (!fileSystem.WriteTextFile(virtualPath, std::string(out.c_str()) + "\n"))
        {
            LOGERROR("[Project] Cannot write " + virtualPath + ".");
            return false;
        }
        CleanDirtyState();
        return true;
    }

    bool ProjectSettings::IsDirty() const { return m_IsDirty; }

    void ProjectSettings::CleanDirtyState() { m_IsDirty = false; }
}

#include "HedgehogSettings/api/PlayerSettings.hpp"

#include "HedgehogSettings/api/ProjectSettings.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "Logger/api/Logger.hpp"

#include "yaml-cpp/yaml.h"

#include <cmath>
#include <cstdint>

namespace HedgehogSettings
{
    namespace
    {
        constexpr std::array<std::string_view, 3> WINDOW_MODE_NAMES = { "Windowed", "Borderless", "Fullscreen" };
        constexpr std::array<std::string_view, 3> QUALITY_NAMES     = { "Low", "Medium", "High" };

        // A value outside the enum (a corrupt struct) is written as its first name rather than read out of bounds.
        template <typename Enum, size_t Count>
        std::string NameOf(const std::array<std::string_view, Count>& names, Enum value)
        {
            const size_t index = static_cast<size_t>(value);
            return std::string(names[index < Count ? index : 0]);
        }

        std::string Describe(const YAML::Node& node)
        {
            return node.IsScalar() ? node.Scalar() : std::string("<not a value>");
        }

        void Refuse(const std::string& key, const YAML::Node& node, const std::string& why)
        {
            LOGWARNING("[PlayerSettings] " + key + ": '" + Describe(node) + "' is not " + why +
                       "; it keeps its default.");
        }

        template<typename TEnum, size_t N>
        void ReadEnum(const YAML::Node&                          root,
                      const char*                                key,
                      const std::array<std::string_view, N>&     names,
                      TEnum&                                     field)
        {
            const YAML::Node node = root[key];
            if (!node)
            {
                return;
            }
            if (node.IsScalar())
            {
                for (size_t i = 0; i < N; ++i)
                {
                    if (node.Scalar() == names[i])
                    {
                        field = static_cast<TEnum>(i);
                        return;
                    }
                }
            }
            Refuse(key,
                   node,
                   std::string("one of ") + std::string(names.front()) + " .. " + std::string(names.back()));
        }

        void ReadBool(const YAML::Node& root, const char* key, bool& field)
        {
            const YAML::Node node = root[key];
            if (!node)
            {
                return;
            }
            bool value = false;
            if (node.IsScalar() && YAML::convert<bool>::decode(node, value))
            {
                field = value;
            }
            else
            {
                Refuse(key, node, "true or false");
            }
        }

        // A whole number, read as int64_t so a negative one is range-checked rather than wrapped.
        bool ReadWhole(const YAML::Node& node, int64_t& value)
        {
            return node.IsScalar() && YAML::convert<int64_t>::decode(node, value);
        }

        void ReadSize(const YAML::Node& root, const char* key, uint32_t& field)
        {
            const YAML::Node node = root[key];
            if (!node)
            {
                return;
            }
            int64_t value = 0;
            const bool whole = ReadWhole(node, value);
            const bool inRange =
                value >= ProjectSettings::MIN_WINDOW_SIZE && value <= ProjectSettings::MAX_WINDOW_SIZE;
            if (whole && (value == 0 || inRange))
            {
                field = static_cast<uint32_t>(value);
            }
            else
            {
                Refuse(key, node, "0 or a size from 64 to 16384");
            }
        }

        void ReadMaxFps(const YAML::Node& root, const char* key, int& field)
        {
            const YAML::Node node = root[key];
            if (!node)
            {
                return;
            }
            int64_t value = 0;
            const bool whole = ReadWhole(node, value);
            const bool inRange =
                value >= ProjectSettings::MIN_FRAME_RATE_CAP && value <= ProjectSettings::MAX_FRAME_RATE_CAP;
            if (whole && (value == ProjectSettings::NO_FRAME_RATE_CAP || inRange))
            {
                field = static_cast<int>(value);
            }
            else
            {
                Refuse(key, node, "0 or a rate from 15 to 1000");
            }
        }

        void ReadVolume(const YAML::Node& node, const std::string& key, float& field)
        {
            float value = 0.0f;
            if (node.IsScalar() && YAML::convert<float>::decode(node, value) && std::isfinite(value) &&
                value >= 0.0f && value <= 1.0f)
            {
                field = value;
            }
            else
            {
                Refuse(key, node, "a volume from 0 to 1");
            }
        }

        void ReadBusVolumes(const YAML::Node& root, std::array<float, PLAYER_AUDIO_BUS_COUNT>& volumes)
        {
            const YAML::Node buses = root["bus_volumes"];
            if (!buses)
            {
                return;
            }
            if (!buses.IsMap())
            {
                Refuse("bus_volumes", buses, "a map of bus volumes");
                return;
            }
            for (const auto& entry : buses)
            {
                const std::string name  = entry.first.as<std::string>();
                bool              known = false;
                for (size_t i = 0; i < PLAYER_AUDIO_BUS_COUNT; ++i)
                {
                    if (name == PLAYER_AUDIO_BUSES[i])
                    {
                        ReadVolume(entry.second, "bus_volumes." + name, volumes[i]);
                        known = true;
                    }
                }
                if (!known)
                {
                    LOGWARNING("[PlayerSettings] bus_volumes: '" + name + "' is not an audio bus; it is ignored.");
                }
            }
        }
    }

    PlayerSettings MakeDefaultPlayerSettings(const ProjectSettings& project)
    {
        PlayerSettings settings;
        settings.WindowMode = project.IsFullscreen() ? PlayerWindowMode::Fullscreen : PlayerWindowMode::Windowed;
        settings.VSync      = project.IsVSync();
        settings.MaxFps     = project.GetMaxFps();
        return settings;
    }

    bool LoadPlayerSettings(const FS::FileSystemManager& fileSystem,
                            PlayerSettings&              settings,
                            const std::string&           virtualPath)
    {
        if (!fileSystem.Exists(virtualPath))
        {
            return false;
        }
        const auto text = fileSystem.ReadTextFile(virtualPath);
        if (!text)
        {
            LOGERROR("[PlayerSettings] " + virtualPath + " cannot be read, keeping the settings as they were.");
            return false;
        }

        PlayerSettings loaded = settings;
        try
        {
            const YAML::Node root = YAML::Load(*text);
            if (!root.IsMap())
            {
                LOGERROR("[PlayerSettings] " + virtualPath + " is not a map, keeping the settings as they were.");
                return false;
            }
            ReadEnum(root, "window_mode", WINDOW_MODE_NAMES, loaded.WindowMode);
            ReadSize(root, "width", loaded.Width);
            ReadSize(root, "height", loaded.Height);
            ReadBool(root, "vsync", loaded.VSync);
            ReadMaxFps(root, "max_fps", loaded.MaxFps);
            if (const YAML::Node master = root["master_volume"])
            {
                ReadVolume(master, "master_volume", loaded.MasterVolume);
            }
            ReadBusVolumes(root, loaded.BusVolumes);
            ReadEnum(root, "quality", QUALITY_NAMES, loaded.Quality);
        }
        catch (const YAML::Exception& e)
        {
            LOGERROR("[PlayerSettings] " + virtualPath + " is malformed, keeping the settings as they were: " +
                     e.what());
            return false;
        }

        settings = loaded;
        return true;
    }

    bool SavePlayerSettings(const FS::FileSystemManager& fileSystem, const PlayerSettings& settings,
                            const std::string& virtualPath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "window_mode" << YAML::Value
            << NameOf(WINDOW_MODE_NAMES, settings.WindowMode);
        out << YAML::Key << "width" << YAML::Value << settings.Width;
        out << YAML::Key << "height" << YAML::Value << settings.Height;
        out << YAML::Key << "vsync" << YAML::Value << settings.VSync;
        out << YAML::Key << "max_fps" << YAML::Value << settings.MaxFps;
        out << YAML::Key << "master_volume" << YAML::Value << settings.MasterVolume;
        out << YAML::Key << "bus_volumes" << YAML::Value << YAML::BeginMap;
        for (size_t i = 0; i < PLAYER_AUDIO_BUS_COUNT; ++i)
        {
            out << YAML::Key << std::string(PLAYER_AUDIO_BUSES[i]) << YAML::Value << settings.BusVolumes[i];
        }
        out << YAML::EndMap;
        out << YAML::Key << "quality" << YAML::Value << NameOf(QUALITY_NAMES, settings.Quality);
        out << YAML::EndMap;

        if (!fileSystem.WriteTextFile(virtualPath, std::string(out.c_str()) + "\n"))
        {
            LOGERROR("[PlayerSettings] Cannot write " + virtualPath + ".");
            return false;
        }
        return true;
    }
}

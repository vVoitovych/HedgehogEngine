#pragma once

#include "HedgehogSettingsApi.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace FS
{
    class FileSystemManager;
}

namespace HedgehogSettings
{
    class ProjectSettings;

    // Same order as HW::WindowMode; declared here so HedgehogSettings needs no HedgehogWindow header.
    enum class PlayerWindowMode
    {
        Windowed,
        Borderless,
        Fullscreen
    };

    enum class PlayerQuality
    {
        Low,
        Medium,
        High
    };

    // The audio epic's fixed buses; Master is MasterVolume. Declared here so HedgehogSettings need not
    // link HedgehogAudio.
    inline constexpr size_t PLAYER_AUDIO_BUS_COUNT = 4;
    inline constexpr std::array<std::string_view, PLAYER_AUDIO_BUS_COUNT> PLAYER_AUDIO_BUSES = {
        "Music", "Sfx", "Voice", "Ui"
    };

    inline constexpr const char* PLAYER_SETTINGS_PATH = "player://player_settings.yaml";

    // What the player chose (player://player_settings.yaml, a per-user file outside the project).
    struct PlayerSettings
    {
        PlayerWindowMode                          WindowMode   = PlayerWindowMode::Windowed;
        uint32_t                                  Width        = 0; // 0: the project's size; else MIN_WINDOW_SIZE..MAX
        uint32_t                                  Height       = 0;
        bool                                      VSync        = true;
        int                                       MaxFps       = 0; // NO_FRAME_RATE_CAP or MIN..MAX_FRAME_RATE_CAP
        float                                     MasterVolume = 1.0f;
        std::array<float, PLAYER_AUDIO_BUS_COUNT> BusVolumes   = { 1.0f, 1.0f, 1.0f, 1.0f };
        PlayerQuality                             Quality      = PlayerQuality::High;

        bool operator==(const PlayerSettings&) const = default;
    };

    // Windowed or Borderless from the project, its vsync and frame cap; size 0; volumes 1; High.
    [[nodiscard]] HEDGEHOG_SETTINGS_API PlayerSettings MakeDefaultPlayerSettings(const ProjectSettings& project);

    // settings holds the defaults on entry; each key read replaces its field, a bad one keeps it with
    // one [PlayerSettings] warning. False, settings unchanged, for a missing file; false with one error
    // for a malformed one.
    [[nodiscard]] HEDGEHOG_SETTINGS_API bool LoadPlayerSettings(const FS::FileSystemManager& fileSystem,
                                                                PlayerSettings&              settings,
                                                                const std::string& virtualPath = PLAYER_SETTINGS_PATH);

    [[nodiscard]] HEDGEHOG_SETTINGS_API bool SavePlayerSettings(const FS::FileSystemManager& fileSystem,
                                                                const PlayerSettings&        settings,
                                                                const std::string& virtualPath = PLAYER_SETTINGS_PATH);
}

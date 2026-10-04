#pragma once

#include "HedgehogSettingsApi.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace FS
{
    class FileSystemManager;
}

namespace HedgehogSettings
{
    // What a project is, as a game build runs it (engine://Project.yaml): its name (the folder its
    // saves go in), the scene a game starts with, the game window, and the game's save data version.
    // The editor edits it in File > Project Settings; the game runtime reads it at start.
    //
    // Every setter validates: a refused value is logged and leaves the setting as it was. A setter
    // that changes a value marks the settings dirty until CleanDirtyState (Load and Save clean it).
    class ProjectSettings
    {
    public:
        static constexpr const char* PATH                  = "engine://Project.yaml";
        static constexpr const char* DEFAULT_NAME          = "HedgehogEngine";
        static constexpr size_t      MAX_NAME_LENGTH       = 64;
        static constexpr uint32_t    DEFAULT_WINDOW_WIDTH  = 1366;
        static constexpr uint32_t    DEFAULT_WINDOW_HEIGHT = 768;
        static constexpr uint32_t    MIN_WINDOW_SIZE       = 64;
        static constexpr uint32_t    MAX_WINDOW_SIZE       = 16384;
        static constexpr int         DEFAULT_GAME_DATA_VERSION = 1;

        HEDGEHOG_SETTINGS_API ProjectSettings();

        // 1 to 64 of A-Z, a-z, 0-9, '_', '-' and spaces, not starting or ending with a space: one
        // plain folder name on every platform.
        [[nodiscard]] HEDGEHOG_SETTINGS_API static bool IsValidName(std::string_view name);
        // Empty (no startup scene), or a .yaml file under assets:// with no ".." segment.
        [[nodiscard]] HEDGEHOG_SETTINGS_API static bool IsValidStartupScene(std::string_view virtualPath);

        [[nodiscard]] HEDGEHOG_SETTINGS_API const std::string& GetName() const;
        HEDGEHOG_SETTINGS_API bool SetName(const std::string& name);

        // A virtual path under assets://, or empty when the project names none. Backslashes are
        // stored as slashes.
        [[nodiscard]] HEDGEHOG_SETTINGS_API const std::string& GetStartupScene() const;
        HEDGEHOG_SETTINGS_API bool SetStartupScene(const std::string& virtualPath);

        // The game window: its title (empty uses the project's name), client size in pixels
        // (clamped to MIN_WINDOW_SIZE..MAX_WINDOW_SIZE), fullscreen and vsync.
        [[nodiscard]] HEDGEHOG_SETTINGS_API const std::string& GetWindowTitle() const;
        HEDGEHOG_SETTINGS_API void SetWindowTitle(const std::string& title);
        [[nodiscard]] HEDGEHOG_SETTINGS_API uint32_t GetWindowWidth() const;
        [[nodiscard]] HEDGEHOG_SETTINGS_API uint32_t GetWindowHeight() const;
        HEDGEHOG_SETTINGS_API void SetWindowSize(uint32_t width, uint32_t height);
        [[nodiscard]] HEDGEHOG_SETTINGS_API bool IsFullscreen() const;
        HEDGEHOG_SETTINGS_API void SetFullscreen(bool fullscreen);
        [[nodiscard]] HEDGEHOG_SETTINGS_API bool IsVSync() const;
        HEDGEHOG_SETTINGS_API void SetVSync(bool vsync);

        // Written into every save game. A game bumps it when the data its saves hold changes
        // shape; older saves are then migrated on load and newer ones refused. At least 1; a
        // smaller value is clamped.
        [[nodiscard]] HEDGEHOG_SETTINGS_API int GetGameDataVersion() const;
        HEDGEHOG_SETTINGS_API void SetGameDataVersion(int version);

        // Replaces every setting with the file's, starting from the defaults; a key that is
        // missing keeps its default, and one whose value is refused keeps it too, with a warning.
        // A missing file is not an error: the defaults stand and it returns false. A malformed
        // one logs an error, keeps the settings as they were and returns false.
        [[nodiscard]] HEDGEHOG_SETTINGS_API bool Load(const std::string& virtualPath, const FS::FileSystemManager& fileSystem);
        [[nodiscard]] HEDGEHOG_SETTINGS_API bool Save(const std::string& virtualPath, const FS::FileSystemManager& fileSystem);

        [[nodiscard]] HEDGEHOG_SETTINGS_API bool IsDirty() const;
        HEDGEHOG_SETTINGS_API void CleanDirtyState();

    private:
        std::string m_Name         = DEFAULT_NAME;
        std::string m_StartupScene;
        std::string m_WindowTitle;
        uint32_t    m_WindowWidth  = DEFAULT_WINDOW_WIDTH;
        uint32_t    m_WindowHeight = DEFAULT_WINDOW_HEIGHT;
        bool        m_Fullscreen   = false;
        bool        m_VSync        = true;
        int         m_GameDataVersion = DEFAULT_GAME_DATA_VERSION;
        bool        m_IsDirty      = false;
    };
}

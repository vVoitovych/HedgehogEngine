#pragma once

#include "../api/HedgehogSettingsApi.hpp"

#include <memory>

namespace HedgehogSettings
{
    class ShadowmapSettings;

    class Settings
    {
    public:
        HEDGEHOG_SETTINGS_API Settings();
        HEDGEHOG_SETTINGS_API ~Settings();

        Settings(const Settings&) = delete;
        Settings(Settings&&) = delete;
        Settings& operator=(const Settings&) = delete;
        Settings& operator=(Settings&&) = delete;

        HEDGEHOG_SETTINGS_API std::unique_ptr<ShadowmapSettings>& GetShadowmapSettings();
        HEDGEHOG_SETTINGS_API const std::unique_ptr<ShadowmapSettings>& GetShadowmapSettings() const;
        HEDGEHOG_SETTINGS_API bool IsDirty() const;
        HEDGEHOG_SETTINGS_API void CleanDirtyState();

    private:
        std::unique_ptr<ShadowmapSettings> m_ShadowmapSettings;

    };
}



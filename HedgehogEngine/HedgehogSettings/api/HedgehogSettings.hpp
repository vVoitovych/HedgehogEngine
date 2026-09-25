#pragma once

#include "HedgehogSettingsApi.hpp"

#include <memory>
#include <string>

namespace FS
{
    class FileSystemManager;
}

namespace HedgehogSettings
{
    class ShadowmapSettings;
    class LayerSettings;
    class RenderingSettings;

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

        HEDGEHOG_SETTINGS_API std::unique_ptr<LayerSettings>& GetLayerSettings();
        HEDGEHOG_SETTINGS_API const std::unique_ptr<LayerSettings>& GetLayerSettings() const;

        HEDGEHOG_SETTINGS_API RenderingSettings&       GetRenderingSettings();
        HEDGEHOG_SETTINGS_API const RenderingSettings& GetRenderingSettings() const;

        // Engine settings live in their own file rather than the editor's layout file, so a game
        // build can read them too. A missing file is not an error: the defaults stand and Load
        // returns false.
        //
        // Load at startup BEFORE the renderer is constructed — these values decide the size of
        // the GPU resources it creates — and call CleanDirtyState() afterwards, since there are
        // no resources to resize yet. Load itself leaves the dirty flags as the setters left
        // them, so a reload at runtime still triggers the resize it should.
        [[nodiscard]] HEDGEHOG_SETTINGS_API bool Load(const std::string& virtualPath,
                                                      const FS::FileSystemManager& fileSystem);
        [[nodiscard]] HEDGEHOG_SETTINGS_API bool Save(const std::string& virtualPath,
                                                      const FS::FileSystemManager& fileSystem) const;

        // "Do the renderer's GPU resources need rebuilding?" — not "has any setting changed".
        // Renderer::DrawFrame gates resource recreation on this, so purely cosmetic settings
        // (layer names) deliberately do not participate. Query those sub-objects directly.
        HEDGEHOG_SETTINGS_API bool IsDirty() const;
        HEDGEHOG_SETTINGS_API void CleanDirtyState();

    private:
        std::unique_ptr<ShadowmapSettings> m_ShadowmapSettings;
        std::unique_ptr<LayerSettings>     m_LayerSettings;
        std::unique_ptr<RenderingSettings> m_RenderingSettings;
    };
}

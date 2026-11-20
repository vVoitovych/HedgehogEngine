#pragma once

#include "HedgehogSettingsApi.hpp"
#include "PlayerSettings.hpp"

#include <cstdint>

namespace HedgehogSettings
{
    class ShadowmapSettings;

    // The project's own shadow values, which High keeps and the lower levels scale down from.
    struct ShadowQualityBase
    {
        uint32_t ShadowmapSize = 2048;
        uint32_t CascadesCount = 4;
        uint32_t PcfRadius     = 1;
    };

    [[nodiscard]] HEDGEHOG_SETTINGS_API ShadowQualityBase CaptureShadowQualityBase(const ShadowmapSettings& shadowmap);

    // Only what the renderer can change at runtime (the atlas is declared each frame at its size):
    // High the base, Medium three quarters of its shadow map, Low half of it with at most 2 cascades and
    // no PCF. The settings are the game's in-memory copy; nothing here writes engine_settings.yaml.
    HEDGEHOG_SETTINGS_API void ApplyQualityLevel(ShadowmapSettings&       shadowmap,
                                                 const ShadowQualityBase& base,
                                                 PlayerQuality            quality);
}

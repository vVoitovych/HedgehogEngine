#include "HedgehogSettings/api/QualityLevels.hpp"

#include "HedgehogSettings/api/ShadowmapingSettings.hpp"

#include <algorithm>

namespace HedgehogSettings
{
    ShadowQualityBase CaptureShadowQualityBase(const ShadowmapSettings& shadowmap)
    {
        return { shadowmap.GetShadowmapSize(), shadowmap.GetCascadesCount(), shadowmap.GetPcfRadius() };
    }

    void ApplyQualityLevel(ShadowmapSettings& shadowmap, const ShadowQualityBase& base, PlayerQuality quality)
    {
        uint32_t size     = base.ShadowmapSize;
        uint32_t cascades = base.CascadesCount;
        uint32_t pcf      = base.PcfRadius;
        switch (quality)
        {
        case PlayerQuality::Low:
            size     = base.ShadowmapSize / 2;
            cascades = std::min(cascades, 2u);
            pcf      = 0;
            break;
        case PlayerQuality::Medium:
            size = base.ShadowmapSize / 4 * 3;
            break;
        case PlayerQuality::High:
            break;
        }
        shadowmap.SetShadowmapSize(size);
        shadowmap.SetCascadesCount(cascades);
        shadowmap.SetPcfRadius(pcf);
    }
}

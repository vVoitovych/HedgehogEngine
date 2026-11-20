#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/HedgehogSettings/api/QualityLevels.hpp"
#include "HedgehogEngine/HedgehogSettings/api/ShadowmapingSettings.hpp"

using HedgehogSettings::ApplyQualityLevel;
using HedgehogSettings::CaptureShadowQualityBase;
using HedgehogSettings::PlayerQuality;
using HedgehogSettings::ShadowmapSettings;
using HedgehogSettings::ShadowQualityBase;

namespace
{
    void CheckShadows(const ShadowmapSettings& shadowmap, uint32_t size, uint32_t cascades, uint32_t pcf)
    {
        CHECK(shadowmap.GetShadowmapSize() == size);
        CHECK(shadowmap.GetCascadesCount() == cascades);
        CHECK(shadowmap.GetPcfRadius() == pcf);
    }
}

TEST_CASE("Quality levels - each level scales the project's shadows, and High gives them back")
{
    ShadowmapSettings shadowmap;
    shadowmap.SetShadowmapSize(2048);
    shadowmap.SetCascadesCount(4);
    shadowmap.SetPcfRadius(2);
    const ShadowQualityBase base = CaptureShadowQualityBase(shadowmap);
    CHECK(base.ShadowmapSize == 2048);
    CHECK(base.CascadesCount == 4);
    CHECK(base.PcfRadius == 2);

    ApplyQualityLevel(shadowmap, base, PlayerQuality::Low);
    CheckShadows(shadowmap, 1024, 2, 0);
    ApplyQualityLevel(shadowmap, base, PlayerQuality::Medium);
    CheckShadows(shadowmap, 1536, 4, 2);
    ApplyQualityLevel(shadowmap, base, PlayerQuality::High);
    CheckShadows(shadowmap, 2048, 4, 2);
}

TEST_CASE("Quality levels - Low keeps fewer cascades as they are and never sizes the map to zero")
{
    ShadowmapSettings shadowmap;
    const ShadowQualityBase base{ 1, 1, 0 };

    ApplyQualityLevel(shadowmap, base, PlayerQuality::Low);
    CheckShadows(shadowmap, 1, 1, 0);
    ApplyQualityLevel(shadowmap, base, PlayerQuality::Medium);
    CheckShadows(shadowmap, 1, 1, 0);
}

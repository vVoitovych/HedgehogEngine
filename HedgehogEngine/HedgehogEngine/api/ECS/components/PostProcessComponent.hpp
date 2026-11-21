#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include <string>

namespace HedgehogEngine
{
    enum class PostProcessAntiAliasing
    {
        None = 0,
        Fxaa = 1,
    };

    // How the scene's frame is finished after lighting: anti-aliasing, ambient occlusion, bloom and a
    // colour grading LUT. The first enabled one by entity id is the scene's; with none FXAA alone is
    // on. Like the environment's exposure it lives on the scene, so the Scene and Game views match.
HH_BEGIN_COMPONENT(PostProcessComponent)
    static constexpr const char* kAntiAliasingNames[] = { "None", "Fxaa" };
    HH_PROP_NAMED(bool, Enabled, "Enabled", true, None)
    HH_PROP_NAMED_ENUM(HedgehogEngine::PostProcessAntiAliasing, AntiAliasing, "AntiAliasing",
                       HedgehogEngine::PostProcessAntiAliasing::Fxaa, kAntiAliasingNames, 2)

    HH_PROP_NAMED(bool,  SsaoEnabled,   "SsaoEnabled",   false,  None)
    HH_PROP_NAMED(float, SsaoRadius,    "SsaoRadius",    0.5f,   None) // metres
    HH_PROP_NAMED(float, SsaoIntensity, "SsaoIntensity", 1.0f,   None)
    HH_PROP_NAMED(float, SsaoBias,      "SsaoBias",      0.025f, None)

    HH_PROP_NAMED(bool,  BloomEnabled,   "BloomEnabled",   false, None)
    HH_PROP_NAMED(float, BloomIntensity, "BloomIntensity", 0.05f, None)
    // HDR radiance above which a pixel blooms, faded in over Knee below it.
    HH_PROP_NAMED(float, BloomThreshold, "BloomThreshold", 1.0f,  None)
    HH_PROP_NAMED(float, BloomKnee,      "BloomKnee",      0.5f,  None)

    // A PNG strip of 32 tiles, 1024 x 32, under assets://; empty for no grading.
    HH_PROP_NAMED(std::string, ColorLut,             "ColorLut",             std::string{}, AssetRef)
    HH_PROP_NAMED(float,       ColorLutContribution, "ColorLutContribution", 1.0f,          None)
HH_END_COMPONENT(PostProcessComponent)
}

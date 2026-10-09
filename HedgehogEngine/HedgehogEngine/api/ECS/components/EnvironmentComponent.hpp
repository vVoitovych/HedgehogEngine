#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include <string>

namespace HedgehogEngine
{
    // The scene's surroundings: an HDR environment map (an equirectangular .hdr under assets://) that
    // lights every surface and, with ShowSkybox, is drawn where nothing else is, and the exposure the
    // frame is tone mapped with. The first enabled one by entity id is the scene's; with none the
    // scene is lit by its lights alone and drawn at 0 EV. Exposure lives here rather than on the
    // camera, so the editor's Scene view and the game view match.
HH_BEGIN_COMPONENT(EnvironmentComponent)
    HH_PROP_NAMED(bool,        Enabled,    "Enabled",    true,          None)
    HH_PROP_NAMED(std::string, Map,        "Map",        std::string{}, AssetRef)
    // Times the map's radiance.
    HH_PROP_NAMED(float,       Intensity,  "Intensity",  1.0f,          None)
    // Degrees about +Z, the engine's up.
    HH_PROP_NAMED(float,       Rotation,   "Rotation",   0.0f,          None)
    HH_PROP_NAMED(bool,        ShowSkybox, "ShowSkybox", true,          None)
    // EV: +1 doubles the brightness the frame is tone mapped from.
    HH_PROP_NAMED(float,       Exposure,   "Exposure",   0.0f,          None)
HH_END_COMPONENT(EnvironmentComponent)
}

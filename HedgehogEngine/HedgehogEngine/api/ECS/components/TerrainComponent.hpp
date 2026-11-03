#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include <cstdint>
#include <string>

namespace HedgehogEngine
{
    // One terrain tile (a .terrain file under assets://, TerrainData.hpp) placed at its entity: the
    // tile spans the entity's local x and y from 0 to its size, heights along +Z. While Enabled it is
    // drawn on its render layer and, in Play, is a static body on its physics layer.
HH_BEGIN_COMPONENT(TerrainComponent)
    HH_PROP_NAMED(bool,        Enabled,      "Enabled",      true,          None)
    HH_PROP_NAMED(std::string, Terrain,      "Terrain",      std::string{}, AssetRef)
    // An index into HedgehogSettings::LayerSettings, as RenderComponent's Layer.
    HH_PROP_NAMED(uint32_t,    RenderLayer,  "RenderLayer",  0u,            None)
    HH_PROP_NAMED(bool,        CastShadows,  "CastShadows",  true,          None)
    // A physics layer (engine_settings.yaml's physics.layers), as ColliderComponent's Layer.
    HH_PROP_NAMED_SLIDER(int32_t, PhysicsLayer, "PhysicsLayer", 0, 0, 15)
    HH_PROP_NAMED(float,       Friction,     "Friction",     0.5f,          None)

    // Runtime only, never written: the terrain's index in the TerrainContainer (-1 for none, or a
    // file that does not load) and the Terrain path it was resolved from, so a changed path is
    // resolved again.
    int32_t     TerrainIndex = -1;
    std::string ResolvedTerrain;
HH_END_COMPONENT(TerrainComponent)
}

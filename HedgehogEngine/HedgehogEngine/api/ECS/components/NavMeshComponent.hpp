#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include "HedgehogNavigation/api/NavigationTypes.hpp"

#include <string>

namespace HedgehogEngine
{
    // The scene's navigation mesh: NavMesh names the baked .navmesh file the NavigationSystem loads
    // (the first enabled component's, by entity id), and the agent and grid fields are what a bake of
    // the scene uses (MakeNavMeshBuildSettings).
HH_BEGIN_COMPONENT(NavMeshComponent)
    HH_PROP_NAMED(bool,        Enabled,             "Enabled",             true,          None)
    HH_PROP_NAMED(std::string, NavMesh,             "NavMesh",             std::string{}, AssetRef)
    HH_PROP_NAMED(float,       AgentRadius,         "AgentRadius",         0.4f,          None)
    HH_PROP_NAMED(float,       AgentHeight,         "AgentHeight",         1.8f,          None)
    HH_PROP_NAMED(float,       MaxClimb,            "MaxClimb",            0.4f,          None)
    HH_PROP_NAMED(float,       MaxSlope,            "MaxSlope",            45.0f,         None)
    HH_PROP_NAMED(float,       CellSize,            "CellSize",            0.3f,          None)
    HH_PROP_NAMED(float,       CellHeight,          "CellHeight",          0.2f,          None)
    HH_PROP_NAMED(bool,        IncludeRenderMeshes, "IncludeRenderMeshes", false,         None)

    // Runtime only, never written: whether the NavigationSystem has this component's file loaded.
    bool Loaded = false;
HH_END_COMPONENT(NavMeshComponent)

    // The component's bake settings, the rest at HN's defaults.
    inline HN::NavMeshBuildSettings MakeNavMeshBuildSettings(const NavMeshComponent& component)
    {
        HN::NavMeshBuildSettings settings;
        settings.AgentRadius   = component.AgentRadius;
        settings.AgentHeight   = component.AgentHeight;
        settings.AgentMaxClimb = component.MaxClimb;
        settings.AgentMaxSlope = component.MaxSlope;
        settings.CellSize      = component.CellSize;
        settings.CellHeight    = component.CellHeight;
        return settings;
    }
}

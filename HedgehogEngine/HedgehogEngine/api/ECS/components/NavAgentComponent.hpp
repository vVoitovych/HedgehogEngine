#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>
#include <vector>

namespace HedgehogEngine
{
    enum class NavAgentStatus : uint8_t
    {
        Idle,    // no destination
        Moving,  // walking its path
        Arrived, // reached its destination
        NoPath,  // its destination could not be reached at all
    };

    // An entity the NavAgentSystem walks along the scene's navmesh to a destination set by code
    // (NavAgentSystem::SetDestination). With a CharacterControllerComponent it drives the character;
    // otherwise it moves the entity's Position itself, at the mesh's height.
HH_BEGIN_COMPONENT(NavAgentComponent)
    HH_PROP_NAMED(bool,  Enabled,          "Enabled",          true,   None)
    // Metres per second, and how fast it gets there, in metres per second squared.
    HH_PROP_NAMED(float, Speed,            "Speed",            3.5f,   None)
    HH_PROP_NAMED(float, Acceleration,     "Acceleration",     8.0f,   None)
    // Degrees per second it turns about +Z while FaceMovement is on.
    HH_PROP_NAMED(float, AngularSpeed,     "AngularSpeed",     360.0f, None)
    // How near a corner (and the destination) counts as reached.
    HH_PROP_NAMED(float, StoppingDistance, "StoppingDistance", 0.1f,   None)
    // Turns the entity so its local +Y faces where it walks.
    HH_PROP_NAMED(bool,  FaceMovement,     "FaceMovement",     true,   None)

    // Runtime only, never written.
    HM::Vector3              Destination    = HM::Vector3(0.0f, 0.0f, 0.0f);
    bool                     HasDestination = false;
    std::vector<HM::Vector3> Path;       // the corners, the start first
    size_t                   Corner = 0; // the corner it walks to
    HM::Vector3              Velocity = HM::Vector3(0.0f, 0.0f, 0.0f);
    NavAgentStatus           Status   = NavAgentStatus::Idle;
HH_END_COMPONENT(NavAgentComponent)
}

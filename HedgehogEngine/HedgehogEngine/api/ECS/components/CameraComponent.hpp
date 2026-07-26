#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

namespace HedgehogEngine
{
HH_BEGIN_COMPONENT(CameraComponent)
    HH_PROP_NAMED(bool, IsPrimary, "IsPrimary", false, None)
    HH_PROP_NAMED_SLIDER(float, Fov, "Fov", 45.0f, 10.0f, 120.0f) // vertical FOV, degrees
    HH_PROP_NAMED(float, NearPlane, "NearPlane", 0.1f, None)
    HH_PROP_NAMED(float, FarPlane,  "FarPlane",  1000.0f, None)
HH_END_COMPONENT(CameraComponent)
}

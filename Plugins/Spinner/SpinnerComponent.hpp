#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include "HedgehogMath/api/Vector.hpp"

namespace Spinner
{
    // Turns its entity about Axis (in the entity's own space) at Speed degrees per second while
    // the game plays. Saved in scenes under "SpinnerComponent".
HH_BEGIN_COMPONENT(SpinnerComponent)
    HH_PROP(bool, Enabled, true, None)
    HH_PROP(float, Speed, 90.0f, None)
    HH_PROP(HM::Vector3, Axis, HM::Vector3(0.0f, 1.0f, 0.0f), None)
HH_END_COMPONENT(SpinnerComponent)
}

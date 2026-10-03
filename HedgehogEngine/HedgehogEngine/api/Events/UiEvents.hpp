#pragma once

#include "ECS/api/Entity.hpp"

namespace HedgehogEngine
{
    /// Emitted by UiSystem when a button is clicked in Play mode: the pointer pressed and released
    /// on it, or UiSubmit pressed while it has focus.
    struct UiButtonClickedEvent
    {
        ECS::Entity Entity = 0;
    };
}

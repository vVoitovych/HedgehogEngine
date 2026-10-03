#pragma once

#include "HedgehogInput/api/InputActionMap.hpp"

namespace HInput
{
    // The actions the engine ships with, used when the project has no actions file:
    // - Game: UiNavigateUp/Down/Left/Right (the arrow keys, the D-pad and the left stick past a 0.2
    //   deadzone), UiSubmit (Enter, keypad Enter, Space, the gamepad's A) and UiPointerPress (the left
    //   mouse button);
    // - Editor: EditorCameraForward (W/S), EditorCameraRight (D/A) and EditorCameraUp (E/Q) as key
    //   axes, EditorCameraLookX/Y (the pointer's move, ignoring 2 px or less) and
    //   EditorCameraLookHold (the left, right or middle mouse button).
    [[nodiscard]] InputActionSet MakeDefaultInputActions();
}

#pragma once

#include "HedgehogInput/api/InputActionMap.hpp"

#include "HedgehogEngine/HedgehogWindow/api/RawInput.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <vector>

// One frame's view of a map's actions, evaluated from a RawInput, as plain data and free functions.
namespace HInput
{
    // An action's value at or above this counts as down.
    inline constexpr float PRESS_THRESHOLD = 0.5f;

    struct ActionValue
    {
        float Value    = 0.0f;  // the strongest binding's value: [-1, 1] for keys and buttons, unbounded for pointer and scroll
        bool  Down     = false; // Value >= PRESS_THRESHOLD
        bool  Pressed  = false; // went down this frame
        bool  Released = false; // went up this frame
        bool  Consumed = false; // something already handled it this frame (ConsumeAction)
    };

    // The cursor, in the raw input's coordinates (y down).
    struct PointerState
    {
        HM::Vector2 Position = HM::Vector2(0.0f, 0.0f);
        HM::Vector2 Delta    = HM::Vector2(0.0f, 0.0f);
        bool        Inside   = false;
    };

    // One value per action of the map it was evaluated for, by the action's index.
    struct ActionState
    {
        std::vector<ActionValue> Actions;
        PointerState             Pointer;
    };

    // Evaluates every action of map from input into state, with edges against state's previous
    // frame. A state sized for another map is reset first, so a changed map gives no false edges.
    // Consumption is cleared. Allocates nothing once state is sized for map.
    void UpdateActionState(const InputActionMap& map, const HW::RawInput& input, ActionState& state);

    // Every action up, at zero and unconsumed, and the pointer outside; sized for map.
    void ResetActionState(ActionState& state, const InputActionMap& map);

    // Marks the action handled for the rest of the frame: the queries below then report it up and at
    // zero. An index out of range does nothing.
    void ConsumeAction(ActionState& state, size_t action);

    // An action's state, false or zero once consumed or for an index out of range.
    [[nodiscard]] bool  IsActionDown(const ActionState& state, size_t action);
    [[nodiscard]] bool  WasActionPressed(const ActionState& state, size_t action);
    [[nodiscard]] bool  WasActionReleased(const ActionState& state, size_t action);
    [[nodiscard]] float GetActionValue(const ActionState& state, size_t action);
}

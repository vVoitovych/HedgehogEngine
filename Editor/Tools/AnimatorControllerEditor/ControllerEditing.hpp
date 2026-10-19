#pragma once

#include "HedgehogAnimation/api/AnimatorController.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// The animator controller editor's edits, as free functions over the controller with no ImGui,
// so EditorTest covers them. Each keeps the controller's indices consistent: an edit of a
// controller ValidateAnimatorController accepts leaves one it accepts (bar the clip names, which
// the mesh decides).
namespace Editor
{
    // name when no state (or parameter) has it, else name followed by the first free number
    // ("Idle 1", "Idle 2", ...). An empty name counts as "State" (or "Parameter").
    [[nodiscard]] std::string MakeUniqueStateName(const HedgehogAnimation::AnimatorController& controller,
                                                  std::string_view name);
    [[nodiscard]] std::string MakeUniqueParameterName(const HedgehogAnimation::AnimatorController& controller,
                                                      std::string_view name);

    // A state named after name (made unique) playing clip; the first state is the default.
    // Returns its index.
    uint32_t AddState(HedgehogAnimation::AnimatorController& controller, std::string_view name, std::string_view clip);
    // False, changing nothing, for an index out of range, an empty name or one another state has.
    bool RenameState(HedgehogAnimation::AnimatorController& controller, uint32_t state, std::string_view name);
    // Removes the state and every transition from or to it; later states move down one. A
    // deleted default state makes the first state the default.
    bool DeleteState(HedgehogAnimation::AnimatorController& controller, uint32_t state);
    bool SetDefaultState(HedgehogAnimation::AnimatorController& controller, uint32_t state);

    // A parameter named after name (made unique) of type, at its default. Returns its index.
    uint32_t AddParameter(HedgehogAnimation::AnimatorController& controller, std::string_view name,
                          HedgehogAnimation::AnimatorParameterType type);
    // Renames it, and every state's speed parameter naming it. False as RenameState is.
    bool RenameParameter(HedgehogAnimation::AnimatorController& controller, uint32_t parameter, std::string_view name);
    // Removes it, every condition testing it and every speed parameter naming it; later
    // parameters move down one.
    bool DeleteParameter(HedgehogAnimation::AnimatorController& controller, uint32_t parameter);
    // Changes its type: its default made one of the type (a whole number, 0 or 1, 0 for a
    // trigger), every condition testing it with an operator the type does not take given the
    // type's default operator, and a speed parameter naming it dropped unless it stays a Float.
    bool SetParameterType(HedgehogAnimation::AnimatorController& controller, uint32_t parameter,
                          HedgehogAnimation::AnimatorParameterType type);

    // The operator a new condition on a parameter of type starts with: Greater, True or Triggered.
    [[nodiscard]] HedgehogAnimation::ConditionOp GetDefaultConditionOp(HedgehogAnimation::AnimatorParameterType type);

    // A transition from from (a state or ANY_STATE) to to, last in the list, with an exit time of
    // 1 (a whole play of the clip) and no condition, so it runs as it is. Nullopt for a state out
    // of range.
    std::optional<uint32_t> AddTransition(HedgehogAnimation::AnimatorController& controller, uint32_t from, uint32_t to);
    bool DeleteTransition(HedgehogAnimation::AnimatorController& controller, uint32_t transition);
    // Moves a transition to another place in the list, which is the order they are checked in.
    bool MoveTransition(HedgehogAnimation::AnimatorController& controller, uint32_t transition, uint32_t place);

    // A condition on parameter with its type's default operator, last on the transition. Nullopt
    // for a transition or parameter out of range.
    std::optional<uint32_t> AddCondition(HedgehogAnimation::AnimatorController& controller, uint32_t transition,
                                         uint32_t parameter);
    // Removes a condition. A transition left with neither a condition nor an exit time gets an
    // exit time of 1, so it does not fire at once.
    bool DeleteCondition(HedgehogAnimation::AnimatorController& controller, uint32_t transition, uint32_t condition);
    // Points a condition at another parameter, its operator kept when that type takes it, else
    // the type's default.
    bool SetConditionParameter(HedgehogAnimation::AnimatorController& controller, uint32_t transition,
                               uint32_t condition, uint32_t parameter);
}

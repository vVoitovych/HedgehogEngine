#pragma once

#include "AnimatorController.hpp"

#include <optional>
#include <string>
#include <string_view>

// The text form of an AnimatorController, a .animctrl file, version 1:
//
//   Version: 1
//   Parameters:
//     - Name: Speed
//       Type: Float
//     - Name: Jump
//       Type: Trigger
//   States:
//     - Name: Idle
//       Clip: Idle
//     - Name: Run
//       Clip: Run
//       SpeedParameter: Speed
//   Default: Idle
//   Transitions:
//     - From: Idle
//       To: Run
//       Duration: 0.25
//       Conditions:
//         - { Parameter: Speed, Op: Greater, Value: 0.5 }
//     - From: Any
//       To: Idle
//       ExitTime: 1
//       Conditions:
//         - { Parameter: Jump, Op: Triggered }
//
// States and parameters are named; From is a state or Any. A parameter's Default (0, false) and
// a state's Speed (1), Loop (true) and SpeedParameter (none) may be left out, as may a
// transition's ExitTime (none, so no exit time), Duration (0.25), CanTransitionToSelf (false)
// and Conditions (none). A condition's Value is left out for True, False and Triggered.
namespace HedgehogAnimation
{
    inline constexpr int ANIMATOR_CONTROLLER_VERSION = 1;

    // The extension of an animator controller file.
    inline constexpr std::string_view ANIMATOR_CONTROLLER_EXTENSION = ".animctrl";

    struct AnimatorControllerParseResult
    {
        std::optional<AnimatorController> Controller; // empty on any error
        std::string                       Error;      // one message naming the entry at fault
    };

    // Reads text strictly: malformed YAML, a missing or other Version, an unknown key, type or
    // operator, a state or parameter that does not exist, a value that is not a number (or not a
    // bool for a Bool parameter's default), a missing name, clip, To or Default fails. Whether the
    // controller can run is ValidateAnimatorController's to say.
    [[nodiscard]] AnimatorControllerParseResult ParseAnimatorController(std::string_view text);

    // Writes controller in the layout above, leaving out every value at its default, so writing
    // what ParseAnimatorController read gives the same text again. Indices out of range are
    // written as empty names.
    [[nodiscard]] std::string WriteAnimatorController(const AnimatorController& controller);
}

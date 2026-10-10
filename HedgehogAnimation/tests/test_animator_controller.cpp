#include "doctest/doctest/doctest.h"

#include "HedgehogAnimation/api/AnimatorControllerFile.hpp"

#include <string>
#include <vector>

using namespace HedgehogAnimation;

namespace
{
    // Every kind of entry, each value away from its default, in the order WriteAnimatorController
    // writes it.
    const char* const FULL = R"(Version: 1
Parameters:
  - Name: Speed
    Type: Float
    Default: 0.5
  - Name: Lives
    Type: Int
    Default: 3
  - Name: Grounded
    Type: Bool
    Default: true
  - Name: Jump
    Type: Trigger
States:
  - Name: Idle
    Clip: Bend
  - Name: Run
    Clip: Lift
    Speed: 2
    Loop: false
    SpeedParameter: Speed
Default: Run
Transitions:
  - From: Idle
    To: Run
    Duration: 0.5
    Conditions:
      - {Parameter: Speed, Op: Greater, Value: 1}
      - {Parameter: Lives, Op: NotEquals, Value: 0}
  - From: Any
    To: Idle
    ExitTime: 0.75
    CanTransitionToSelf: true
    Conditions:
      - {Parameter: Grounded, Op: False}
      - {Parameter: Jump, Op: Triggered}
  - From: Run
    To: Idle
    ExitTime: 1
)";

    std::string Error(const std::string& text)
    {
        const AnimatorControllerParseResult result = ParseAnimatorController(text);
        CHECK_FALSE(result.Controller.has_value());
        return result.Error;
    }

    bool Mentions(const std::vector<std::string>& problems, const std::string& fragment)
    {
        for (const std::string& problem : problems)
            if (problem.find(fragment) != std::string::npos)
                return true;
        return false;
    }
}

TEST_CASE("AnimatorControllerFile - every field reads, and writing it back gives the same text")
{
    const AnimatorControllerParseResult result = ParseAnimatorController(FULL);
    REQUIRE_MESSAGE(result.Controller.has_value(), result.Error);
    const AnimatorController& controller = *result.Controller;

    REQUIRE(controller.Parameters.size() == 4u);
    CHECK(controller.Parameters[0].Default == 0.5f);
    CHECK(controller.Parameters[1].Type == AnimatorParameterType::Int);
    CHECK(controller.Parameters[1].Default == 3.0f);
    CHECK(controller.Parameters[2].Default == 1.0f);
    CHECK(controller.Parameters[3].Type == AnimatorParameterType::Trigger);

    REQUIRE(controller.States.size() == 2u);
    CHECK(controller.States[0].Speed == 1.0f);
    CHECK(controller.States[0].Loop);
    CHECK(controller.States[1].Clip == "Lift");
    CHECK(controller.States[1].Speed == 2.0f);
    CHECK_FALSE(controller.States[1].Loop);
    CHECK(controller.States[1].SpeedParameter == "Speed");
    CHECK(controller.DefaultState == 1u);

    REQUIRE(controller.Transitions.size() == 3u);
    const AnimatorTransition& first = controller.Transitions[0];
    CHECK(first.From == 0u);
    CHECK(first.To == 1u);
    CHECK_FALSE(first.HasExitTime);
    CHECK(first.Duration == 0.5f);
    REQUIRE(first.Conditions.size() == 2u);
    CHECK(first.Conditions[1].Parameter == 1u);
    CHECK(first.Conditions[1].Op == ConditionOp::NotEquals);
    const AnimatorTransition& any = controller.Transitions[1];
    CHECK(any.From == ANY_STATE);
    CHECK(any.HasExitTime);
    CHECK(any.ExitTime == 0.75f);
    CHECK(any.Duration == 0.25f); // the default
    CHECK(any.CanTransitionToSelf);
    CHECK(controller.Transitions[2].Conditions.empty());

    CHECK(WriteAnimatorController(controller) == FULL);
    CHECK(ValidateAnimatorController(controller).empty());
}

TEST_CASE("AnimatorControllerFile - a bare controller writes empty lists and reads back")
{
    AnimatorController controller;
    controller.States.push_back(AnimatorState{ "Idle", "Bend" });
    const std::string text = WriteAnimatorController(controller);
    const AnimatorControllerParseResult result = ParseAnimatorController(text);
    REQUIRE_MESSAGE(result.Controller.has_value(), result.Error);
    CHECK(result.Controller->Parameters.empty());
    CHECK(result.Controller->Transitions.empty());
    CHECK(WriteAnimatorController(*result.Controller) == text);
}

TEST_CASE("AnimatorControllerFile - each mistake fails with one message naming it")
{
    const std::string header = "Version: 1\nParameters:\n  - {Name: Speed, Type: Float}\n  - {Name: Go, Type: Trigger}\n"
                               "States:\n  - {Name: Idle, Clip: Bend}\nDefault: Idle\n";

    CHECK_FALSE(Error("Version: [").empty()); // malformed YAML: yaml-cpp's own message
    CHECK(Error("- 1\n- 2\n").find("not a map") != std::string::npos);
    CHECK(Error("States: []\nDefault: Idle\n").find("Version is missing") != std::string::npos);
    CHECK(Error("Version: 2\n").find("Version 2 is not supported") != std::string::npos);
    CHECK(Error("Version: 1\nLayers: []\n").find("unknown key 'Layers'") != std::string::npos);
    CHECK(Error("Version: 1\nStates:\n  - {Name: Idle, Clip: Bend}\n").find("Default is missing") != std::string::npos);
    CHECK(Error("Version: 1\nStates:\n  - {Name: Idle}\nDefault: Idle\n").find("state 0: Clip is missing") !=
          std::string::npos);
    CHECK(Error("Version: 1\nStates:\n  - {Name: Idle, Clip: Bend}\nDefault: Walk\n").find("no state 'Walk'") !=
          std::string::npos);
    CHECK(Error("Version: 1\nParameters:\n  - {Name: A, Type: Vector}\n").find("unknown type 'Vector'") !=
          std::string::npos);
    CHECK(Error("Version: 1\nParameters:\n  - {Name: A, Type: Int, Default: 1.5}\n").find("whole number") !=
          std::string::npos);
    CHECK(Error("Version: 1\nParameters:\n  - {Name: A, Type: Bool, Default: maybe}\n").find("not true or false") !=
          std::string::npos);
    CHECK(Error("Version: 1\nParameters:\n  - {Name: A, Type: Trigger, Default: 1}\n").find("has no default") !=
          std::string::npos);
    CHECK(Error("Version: 1\nStates:\n  - {Name: Idle, Clip: Bend, Speed: fast}\nDefault: Idle\n")
              .find("'fast' is not a number") != std::string::npos);
    CHECK(Error(header + "Transitions:\n  - {From: Idle, To: Run}\n").find("transition 0, To: there is no state 'Run'") !=
          std::string::npos);
    CHECK(Error(header + "Transitions:\n  - From: Any\n    To: Idle\n    Conditions:\n      - {Parameter: Mass, Op: Less, Value: 1}\n")
              .find("transition 0, condition 0: there is no parameter 'Mass'") != std::string::npos);
    CHECK(Error(header + "Transitions:\n  - From: Any\n    To: Idle\n    Conditions:\n      - {Parameter: Speed, Op: Above, Value: 1}\n")
              .find("unknown operator 'Above'") != std::string::npos);
    CHECK(Error(header + "Transitions:\n  - From: Any\n    To: Idle\n    Conditions:\n      - {Parameter: Speed, Op: Greater}\n")
              .find("Greater needs a Value") != std::string::npos);
    CHECK(Error(header + "Transitions:\n  - From: Any\n    To: Idle\n    Conditions:\n      - {Parameter: Go, Op: Triggered, Value: 1}\n")
              .find("Triggered takes no Value") != std::string::npos);
}

TEST_CASE("ValidateAnimatorController - names what cannot run")
{
    AnimatorController controller;
    CHECK(Mentions(ValidateAnimatorController(controller), "has no states"));

    controller.Parameters = { { "Speed", AnimatorParameterType::Float }, { "Speed", AnimatorParameterType::Bool },
                              { "", AnimatorParameterType::Int } };
    controller.States     = { { "Idle", "Bend" }, { "Run", "Jog", 1.0f, true, "Lives" } };
    controller.DefaultState = 5;

    AnimatorTransition noCondition;
    noCondition.From = 0;
    noCondition.To   = 1;
    AnimatorTransition wrongType;
    wrongType.From       = 7;
    wrongType.To         = 9;
    wrongType.Duration   = -1.0f;
    wrongType.Conditions = { { 0, ConditionOp::True }, { 4, ConditionOp::Greater } };
    controller.Transitions = { noCondition, wrongType };

    const std::vector<std::string> clips    = { "Bend", "Lift" };
    const std::vector<std::string> problems = ValidateAnimatorController(controller, &clips);
    CHECK(Mentions(problems, "the default state is not one of its states"));
    CHECK(Mentions(problems, "parameter 'Speed' is named twice"));
    CHECK(Mentions(problems, "parameter 2 has no name"));
    CHECK(Mentions(problems, "state 'Run' plays clip 'Jog', which the mesh does not have"));
    CHECK(Mentions(problems, "state 'Run' takes its speed from 'Lives', which is not a Float parameter"));
    CHECK(Mentions(problems, "transition 0 has neither a condition nor an exit time"));
    CHECK(Mentions(problems, "transition 1 leaves a state that does not exist"));
    CHECK(Mentions(problems, "transition 1 leads to a state that does not exist"));
    CHECK(Mentions(problems, "transition 1 has a duration"));
    CHECK(Mentions(problems, "transition 1, condition 0: True cannot test Float parameter 'Speed'"));
    CHECK(Mentions(problems, "transition 1, condition 1 tests a parameter that does not exist"));
    CHECK(problems.size() == 11u);

    // Without the mesh's clip names the clips are not checked.
    CHECK_FALSE(Mentions(ValidateAnimatorController(controller), "Jog"));
}

TEST_CASE("IsConditionOpValid - each type takes its own operators")
{
    CHECK(IsConditionOpValid(AnimatorParameterType::Float, ConditionOp::Greater));
    CHECK_FALSE(IsConditionOpValid(AnimatorParameterType::Float, ConditionOp::Equals));
    CHECK(IsConditionOpValid(AnimatorParameterType::Int, ConditionOp::Equals));
    CHECK(IsConditionOpValid(AnimatorParameterType::Int, ConditionOp::NotEquals));
    CHECK_FALSE(IsConditionOpValid(AnimatorParameterType::Int, ConditionOp::True));
    CHECK(IsConditionOpValid(AnimatorParameterType::Bool, ConditionOp::False));
    CHECK_FALSE(IsConditionOpValid(AnimatorParameterType::Bool, ConditionOp::Triggered));
    CHECK(IsConditionOpValid(AnimatorParameterType::Trigger, ConditionOp::Triggered));
    CHECK_FALSE(IsConditionOpValid(AnimatorParameterType::Trigger, ConditionOp::True));
}

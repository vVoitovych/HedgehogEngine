#include "Tools/AnimatorControllerEditor/ControllerEditing.hpp"
#include "Tools/AnimatorControllerEditor/ControllerLayout.hpp"

#include "FileSystem/tests/test_helpers.hpp"

#include "doctest/doctest/doctest.h"

#include <string>
#include <vector>

using namespace Editor;
using HedgehogAnimation::AnimatorController;
using HedgehogAnimation::AnimatorParameterType;
using HedgehogAnimation::ConditionOp;

namespace
{
    // Idle, Run (its speed from Speed) and Jump; Idle <-> Run on Speed, Any -> Jump on the Jump
    // trigger while Grounded, Jump -> Idle at its end.
    AnimatorController MakeController()
    {
        AnimatorController controller;
        AddParameter(controller, "Speed", AnimatorParameterType::Float);
        AddParameter(controller, "Grounded", AnimatorParameterType::Bool);
        AddParameter(controller, "Jump", AnimatorParameterType::Trigger);
        AddParameter(controller, "Combo", AnimatorParameterType::Int);
        AddState(controller, "Idle", "Idle");
        AddState(controller, "Run", "Run");
        AddState(controller, "Jump", "Jump");
        controller.States[1].SpeedParameter = "Speed";

        const uint32_t toRun = *AddTransition(controller, 0, 1);
        controller.Transitions[toRun].HasExitTime = false;
        AddCondition(controller, toRun, 0);
        controller.Transitions[toRun].Conditions[0].Value = 0.5f;
        const uint32_t toIdle = *AddTransition(controller, 1, 0);
        controller.Transitions[toIdle].HasExitTime = false;
        AddCondition(controller, toIdle, 0);
        controller.Transitions[toIdle].Conditions[0].Op = ConditionOp::Less;
        const uint32_t toJump = *AddTransition(controller, HedgehogAnimation::ANY_STATE, 2);
        controller.Transitions[toJump].HasExitTime = false;
        AddCondition(controller, toJump, 2);
        AddCondition(controller, toJump, 1);
        AddTransition(controller, 2, 0);
        return controller;
    }

    void CheckValid(const AnimatorController& controller)
    {
        const std::vector<std::string> problems = HedgehogAnimation::ValidateAnimatorController(controller);
        for (const std::string& problem : problems)
            FAIL_CHECK(problem);
    }
}

TEST_CASE("ControllerEditing - the test controller is valid, and new names are unique")
{
    AnimatorController controller = MakeController();
    CheckValid(controller);
    CHECK(controller.DefaultState == 0u);
    CHECK(MakeUniqueStateName(controller, "Walk") == "Walk");
    CHECK(MakeUniqueStateName(controller, "Idle") == "Idle 1");
    CHECK(MakeUniqueStateName(controller, "") == "State");
    CHECK(MakeUniqueParameterName(controller, "Speed") == "Speed 1");
    CHECK(AddState(controller, "Idle", "Wave") == 3u);
    CHECK(controller.States[3].Name == "Idle 1");
    CHECK(controller.States[3].Clip == "Wave");
    CHECK(MakeUniqueStateName(controller, "Idle") == "Idle 2");
    CheckValid(controller);
}

TEST_CASE("ControllerEditing - renaming states and parameters")
{
    AnimatorController controller = MakeController();
    CHECK(RenameState(controller, 1, "Sprint"));
    CHECK(controller.States[1].Name == "Sprint");
    CHECK_FALSE(RenameState(controller, 1, "Idle")); // taken
    CHECK_FALSE(RenameState(controller, 1, ""));
    CHECK_FALSE(RenameState(controller, 9, "Walk"));
    CHECK(RenameState(controller, 1, "Sprint")); // its own name

    CHECK(RenameParameter(controller, 0, "Velocity"));
    CHECK(controller.States[1].SpeedParameter == "Velocity"); // followed
    CHECK_FALSE(RenameParameter(controller, 0, "Grounded"));
    CheckValid(controller);
}

TEST_CASE("ControllerEditing - deleting a state takes its transitions and keeps the indices right")
{
    AnimatorController controller = MakeController();
    REQUIRE(SetDefaultState(controller, 2));
    CHECK(DeleteState(controller, 1)); // Run: Idle <-> Run go
    REQUIRE(controller.States.size() == 2u);
    CHECK(controller.States[1].Name == "Jump");
    CHECK(controller.DefaultState == 1u); // Jump moved down one
    REQUIRE(controller.Transitions.size() == 2u);
    CHECK(controller.Transitions[0].From == HedgehogAnimation::ANY_STATE);
    CHECK(controller.Transitions[0].To == 1u);
    CHECK(controller.Transitions[1].From == 1u);
    CHECK(controller.Transitions[1].To == 0u);
    CheckValid(controller);

    CHECK(DeleteState(controller, 1)); // the default: the first state takes over
    CHECK(controller.DefaultState == 0u);
    CHECK(controller.Transitions.empty());
    CheckValid(controller);
    CHECK_FALSE(DeleteState(controller, 5));
    CHECK_FALSE(SetDefaultState(controller, 5));
}

TEST_CASE("ControllerEditing - deleting a parameter takes its conditions and speed parameter")
{
    AnimatorController controller = MakeController();
    CHECK(DeleteParameter(controller, 0)); // Speed
    CHECK(controller.States[1].SpeedParameter.empty());
    // Idle <-> Run lost their only condition, so they wait for their clip instead.
    CHECK(controller.Transitions[0].Conditions.empty());
    CHECK(controller.Transitions[0].HasExitTime);
    CHECK(controller.Transitions[0].ExitTime == 1.0f);
    // Any -> Jump keeps both of its conditions, their parameters moved down one.
    REQUIRE(controller.Transitions[2].Conditions.size() == 2u);
    CHECK(controller.Parameters[controller.Transitions[2].Conditions[0].Parameter].Name == "Jump");
    CHECK(controller.Parameters[controller.Transitions[2].Conditions[1].Parameter].Name == "Grounded");
    CheckValid(controller);
}

TEST_CASE("ControllerEditing - retyping a parameter fixes its default, conditions and speed use")
{
    AnimatorController controller = MakeController();
    controller.Parameters[0].Default = 2.6f;
    CHECK(SetParameterType(controller, 0, AnimatorParameterType::Int)); // Greater and Less stay
    CHECK(controller.Parameters[0].Default == 3.0f);
    CHECK(controller.Transitions[1].Conditions[0].Op == ConditionOp::Less);
    CHECK(controller.States[1].SpeedParameter.empty()); // only a Float speeds a state
    CheckValid(controller);

    CHECK(SetParameterType(controller, 0, AnimatorParameterType::Bool));
    CHECK(controller.Parameters[0].Default == 1.0f);
    CHECK(controller.Transitions[0].Conditions[0].Op == ConditionOp::True);
    CheckValid(controller);

    CHECK(SetParameterType(controller, 1, AnimatorParameterType::Trigger));
    CHECK(controller.Parameters[1].Default == 0.0f);
    CHECK(controller.Transitions[2].Conditions[1].Op == ConditionOp::Triggered);
    CheckValid(controller);
    CHECK_FALSE(SetParameterType(controller, 9, AnimatorParameterType::Float));
}

TEST_CASE("ControllerEditing - transitions and conditions")
{
    AnimatorController controller = MakeController();
    const auto added = AddTransition(controller, 1, 2);
    REQUIRE(added.has_value());
    CHECK(controller.Transitions[*added].HasExitTime); // valid as it is
    CheckValid(controller);
    CHECK_FALSE(AddTransition(controller, 1, 7).has_value());
    CHECK_FALSE(AddTransition(controller, 7, 1).has_value());

    // Checked in list order: move the new one to the front, then back.
    CHECK(MoveTransition(controller, *added, 0));
    CHECK(controller.Transitions[0].From == 1u);
    CHECK(controller.Transitions[0].To == 2u);
    CHECK(controller.Transitions[1].From == 0u); // the rest shifted down, in order
    CHECK(MoveTransition(controller, 0, *added));
    CHECK(controller.Transitions[*added].To == 2u);
    CHECK(controller.Transitions[0].From == 0u);
    CHECK_FALSE(MoveTransition(controller, 0, 99));

    // A condition starts with its type's default operator and follows another parameter.
    const auto condition = AddCondition(controller, *added, 3);
    REQUIRE(condition.has_value());
    CHECK(controller.Transitions[*added].Conditions[*condition].Op == ConditionOp::Greater);
    controller.Transitions[*added].Conditions[*condition].Op = ConditionOp::Equals;
    CHECK(SetConditionParameter(controller, *added, *condition, 0)); // Float: no Equals
    CHECK(controller.Transitions[*added].Conditions[*condition].Op == ConditionOp::Greater);
    CHECK(SetConditionParameter(controller, *added, *condition, 1));
    CHECK(controller.Transitions[*added].Conditions[*condition].Op == ConditionOp::True);
    CheckValid(controller);

    // The last condition of a transition without exit time: it waits for its clip instead.
    CHECK(DeleteCondition(controller, 0, 0));
    CHECK(controller.Transitions[0].HasExitTime);
    CHECK_FALSE(DeleteCondition(controller, 0, 0));
    CheckValid(controller);

    CHECK(DeleteTransition(controller, 0));
    CHECK_FALSE(DeleteTransition(controller, 99));
    CHECK_FALSE(AddCondition(controller, 99, 0).has_value());
    CHECK_FALSE(AddCondition(controller, 0, 99).has_value());
    CheckValid(controller);
}

TEST_CASE("ControllerLayout - state nodes follow renames, and the layout file round-trips")
{
    GraphLayout layout;
    layout[std::string(ENTRY_NODE_KEY)]     = { 10.0f, 20.0f };
    layout[std::string(ANY_STATE_NODE_KEY)] = { 10.0f, 120.0f };
    layout[MakeStateNodeKey("Idle")]        = { 200.0f, 20.0f };
    layout[MakeStateNodeKey("Run")]         = { 400.0f, 20.0f };
    CHECK(MakeStateNodeKey("Idle") == "state:Idle");

    RenameStateNode(layout, "Idle", "Rest");
    CHECK(layout.count("state:Idle") == 0u);
    CHECK(layout.at("state:Rest") == GraphNodePosition{ 200.0f, 20.0f });
    RemoveStateNode(layout, "Run");
    CHECK(layout.count("state:Run") == 0u);
    RenameStateNode(layout, "Missing", "Other"); // nothing to move
    CHECK(layout.size() == 3u);

    TempDir                     dir;
    const std::filesystem::path file = GetGraphLayoutFile(dir.Path() / "Strip.animctrl");
    CHECK(file.filename() == "Strip.animctrl.layout");
    REQUIRE(SaveGraphLayout(file, layout));
    const auto loaded = LoadGraphLayout(file);
    REQUIRE(loaded.has_value());
    CHECK(*loaded == layout);
}

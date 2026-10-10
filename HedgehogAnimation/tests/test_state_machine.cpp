#include "doctest/doctest/doctest.h"

#include "HedgehogAnimation/api/StateMachine.hpp"

#include <vector>

using namespace HedgehogAnimation;

namespace
{
    constexpr uint32_t IDLE = 0;
    constexpr uint32_t WALK = 1;
    constexpr uint32_t JUMP = 2;

    // Idle (1 s clip) and Walk (2 s) loop; Jump (0.5 s) plays once. Parameters: Speed (Float),
    // Steps (Int), Grounded (Bool, true) and Jump (Trigger).
    AnimatorController MakeController()
    {
        AnimatorController controller;
        controller.Parameters = { { "Speed", AnimatorParameterType::Float },
                                  { "Steps", AnimatorParameterType::Int },
                                  { "Grounded", AnimatorParameterType::Bool, 1.0f },
                                  { "Jump", AnimatorParameterType::Trigger } };
        controller.States     = { { "Idle", "Idle" }, { "Walk", "Walk" }, { "Jump", "Jump", 1.0f, false } };
        return controller;
    }

    AnimatorTransition When(uint32_t from, uint32_t to, std::vector<TransitionCondition> conditions)
    {
        AnimatorTransition transition;
        transition.From       = from;
        transition.To         = to;
        transition.Conditions = std::move(conditions);
        return transition;
    }

    AnimatorTransition AtExit(uint32_t from, uint32_t to, float exitTime)
    {
        AnimatorTransition transition;
        transition.From        = from;
        transition.To          = to;
        transition.HasExitTime = true;
        transition.ExitTime    = exitTime;
        return transition;
    }

    const std::vector<float> DURATIONS = { 1.0f, 2.0f, 0.5f };
}

TEST_CASE("StateMachine - starts in the default state with the parameters' defaults, and sets them by name and type")
{
    AnimatorController controller = MakeController();
    controller.DefaultState       = WALK;
    StateMachineInstance instance;
    ResetStateMachine(controller, instance);
    CHECK(instance.State == WALK);
    CHECK(instance.StateTime == 0.0f);
    CHECK(GetParameter(controller, instance, "Grounded") == 1.0f);
    CHECK(GetParameter(controller, instance, "Jump") == 0.0f);

    CHECK(SetFloat(controller, instance, "Speed", 2.5f));
    CHECK(SetInt(controller, instance, "Steps", 4));
    CHECK(SetBool(controller, instance, "Grounded", false));
    CHECK(SetTrigger(controller, instance, "Jump"));
    CHECK(GetParameter(controller, instance, "Speed") == 2.5f);
    CHECK(GetParameter(controller, instance, "Steps") == 4.0f);
    CHECK(GetParameter(controller, instance, "Grounded") == 0.0f);
    CHECK(GetParameter(controller, instance, "Jump") == 1.0f);

    // An unknown name or another type changes nothing.
    CHECK_FALSE(SetFloat(controller, instance, "Steps", 9.0f));
    CHECK_FALSE(SetBool(controller, instance, "Jump", true));
    CHECK_FALSE(SetTrigger(controller, instance, "Grounded"));
    CHECK_FALSE(SetInt(controller, instance, "Height", 1));
    CHECK_FALSE(GetParameter(controller, instance, "Height").has_value());
    CHECK(GetParameter(controller, instance, "Steps") == 4.0f);

    ResetStateMachine(controller, instance);
    CHECK(GetParameter(controller, instance, "Speed") == 0.0f);
    CHECK(GetParameter(controller, instance, "Jump") == 0.0f);
}

TEST_CASE("StateMachine - every condition operator")
{
    struct Case
    {
        uint32_t    Parameter;
        ConditionOp Op;
        float       Value;
        float       Holds;
        float       Fails;
    };
    const Case cases[] = {
        { 0, ConditionOp::Greater, 1.0f, 1.5f, 1.0f },    { 0, ConditionOp::Less, 1.0f, 0.5f, 1.0f },
        { 1, ConditionOp::Equals, 3.0f, 3.0f, 2.0f },     { 1, ConditionOp::NotEquals, 3.0f, 2.0f, 3.0f },
        { 2, ConditionOp::True, 0.0f, 1.0f, 0.0f },       { 2, ConditionOp::False, 0.0f, 0.0f, 1.0f },
        { 3, ConditionOp::Triggered, 0.0f, 1.0f, 0.0f },
    };
    for (const Case& test : cases)
    {
        CAPTURE(GetConditionOpName(test.Op));
        AnimatorController controller = MakeController();
        controller.Transitions        = { When(IDLE, WALK, { { test.Parameter, test.Op, test.Value } }) };
        StateMachineInstance instance;
        ResetStateMachine(controller, instance);

        instance.Values[test.Parameter] = test.Fails;
        CHECK_FALSE(StepStateMachine(controller, instance, DURATIONS, 0.1f).has_value());
        instance.Values[test.Parameter] = test.Holds;
        const auto fired = StepStateMachine(controller, instance, DURATIONS, 0.1f);
        REQUIRE(fired.has_value());
        CHECK(fired->From == IDLE);
        CHECK(fired->To == WALK);
        CHECK(fired->Duration == 0.25f);
        CHECK(instance.State == WALK);
        CHECK(instance.StateTime == 0.0f);
    }
}

TEST_CASE("StateMachine - all conditions must hold, and a fired transition unsets the triggers it tested")
{
    AnimatorController controller = MakeController();
    controller.Transitions        = { When(IDLE, JUMP, { { 3, ConditionOp::Triggered }, { 2, ConditionOp::True } }) };
    StateMachineInstance instance;
    ResetStateMachine(controller, instance);

    SetBool(controller, instance, "Grounded", false);
    SetTrigger(controller, instance, "Jump");
    CHECK_FALSE(StepStateMachine(controller, instance, DURATIONS, 0.1f).has_value());
    CHECK(GetParameter(controller, instance, "Jump") == 1.0f); // not used, so still set

    SetBool(controller, instance, "Grounded", true);
    CHECK(StepStateMachine(controller, instance, DURATIONS, 0.1f).has_value());
    CHECK(instance.State == JUMP);
    CHECK(GetParameter(controller, instance, "Jump") == 0.0f);
}

TEST_CASE("StateMachine - exit time on a state that plays once, and in every loop of a looping one")
{
    SUBCASE("once: after that many lengths of its clip")
    {
        AnimatorController controller = MakeController();
        controller.DefaultState       = JUMP;
        controller.Transitions        = { AtExit(JUMP, IDLE, 1.0f) };
        StateMachineInstance instance;
        ResetStateMachine(controller, instance);
        CHECK_FALSE(StepStateMachine(controller, instance, DURATIONS, 0.4f).has_value()); // 0.8 of 0.5 s
        CHECK(StepStateMachine(controller, instance, DURATIONS, 0.2f).has_value());       // 1.2
        CHECK(instance.State == IDLE);
    }
    SUBCASE("looping: past the point in the current loop, or a loop's end crossed")
    {
        AnimatorController controller = MakeController();
        controller.Transitions        = { AtExit(IDLE, WALK, 0.75f) };
        StateMachineInstance instance;
        ResetStateMachine(controller, instance);
        CHECK_FALSE(StepStateMachine(controller, instance, DURATIONS, 0.5f).has_value());
        CHECK(StepStateMachine(controller, instance, DURATIONS, 0.3f).has_value()); // 0.8

        // A step from 0.5 into the next loop (1.1) passes 0.75 though 0.1 < 0.75.
        ResetStateMachine(controller, instance);
        CHECK_FALSE(StepStateMachine(controller, instance, DURATIONS, 0.5f).has_value());
        CHECK(StepStateMachine(controller, instance, DURATIONS, 0.6f).has_value());
    }
    SUBCASE("with a condition: both must hold")
    {
        AnimatorController controller = MakeController();
        AnimatorTransition transition = AtExit(IDLE, WALK, 0.5f);
        transition.Conditions         = { { 0, ConditionOp::Greater, 1.0f } };
        controller.Transitions        = { transition };
        StateMachineInstance instance;
        ResetStateMachine(controller, instance);
        SetFloat(controller, instance, "Speed", 2.0f);
        CHECK_FALSE(StepStateMachine(controller, instance, DURATIONS, 0.25f).has_value());
        CHECK(StepStateMachine(controller, instance, DURATIONS, 0.3f).has_value());
    }
    SUBCASE("a state whose clip is unknown (length 0) has already played through")
    {
        AnimatorController controller = MakeController();
        controller.Transitions        = { AtExit(IDLE, WALK, 1.0f) };
        StateMachineInstance instance;
        ResetStateMachine(controller, instance);
        const std::vector<float> unknown = { 0.0f, 2.0f, 0.5f };
        CHECK(StepStateMachine(controller, instance, unknown, 0.0f).has_value());
    }
}

TEST_CASE("StateMachine - Any State transitions come first, in order, and one transition fires per step")
{
    AnimatorController controller = MakeController();
    controller.Transitions        = { When(IDLE, WALK, { { 0, ConditionOp::Greater, 0.0f } }),
                                      When(ANY_STATE, JUMP, { { 3, ConditionOp::Triggered } }),
                                      When(ANY_STATE, IDLE, { { 3, ConditionOp::Triggered } }),
                                      When(WALK, IDLE, { { 0, ConditionOp::Greater, 0.0f } }) };
    StateMachineInstance instance;
    ResetStateMachine(controller, instance);

    // Both Idle -> Walk and Any -> Jump may fire: the Any State one wins, and only it.
    SetFloat(controller, instance, "Speed", 1.0f);
    SetTrigger(controller, instance, "Jump");
    const auto fired = StepStateMachine(controller, instance, DURATIONS, 0.1f);
    REQUIRE(fired.has_value());
    CHECK(fired->To == JUMP);
    CHECK(instance.State == JUMP);

    // Next step: the trigger is used up and Jump has no transitions of its own; nothing fires.
    CHECK_FALSE(StepStateMachine(controller, instance, DURATIONS, 0.1f).has_value());
}

TEST_CASE("StateMachine - an Any State transition to the current state needs CanTransitionToSelf")
{
    AnimatorController controller = MakeController();
    controller.Transitions        = { When(ANY_STATE, IDLE, { { 2, ConditionOp::True } }) };
    StateMachineInstance instance;
    ResetStateMachine(controller, instance);

    CHECK_FALSE(StepStateMachine(controller, instance, DURATIONS, 0.1f).has_value());
    CHECK(instance.StateTime > 0.0f);

    controller.Transitions[0].CanTransitionToSelf = true;
    const auto fired = StepStateMachine(controller, instance, DURATIONS, 0.1f);
    REQUIRE(fired.has_value());
    CHECK(fired->From == IDLE);
    CHECK(fired->To == IDLE);
    CHECK(instance.StateTime == 0.0f); // started again
}

TEST_CASE("StateMachine - time runs at the state's speed times its speed parameter")
{
    AnimatorController controller          = MakeController();
    controller.States[IDLE].Speed          = 2.0f;
    controller.States[IDLE].SpeedParameter = "Speed";
    StateMachineInstance instance;
    ResetStateMachine(controller, instance);
    SetFloat(controller, instance, "Speed", 0.5f);
    (void)StepStateMachine(controller, instance, DURATIONS, 0.4f);
    CHECK(instance.StateTime == doctest::Approx(0.4f));
    CHECK(GetNormalizedTime(instance, DURATIONS) == doctest::Approx(0.4f));
}

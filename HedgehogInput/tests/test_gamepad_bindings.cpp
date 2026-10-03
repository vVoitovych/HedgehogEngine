#include "doctest/doctest/doctest.h"

#include "HedgehogInput/api/ActionState.hpp"
#include "HedgehogInput/api/DefaultInputActions.hpp"
#include "HedgehogInput/api/GameInputRegion.hpp"
#include "HedgehogInput/api/InputActionFile.hpp"
#include "HedgehogInput/api/InputNames.hpp"

#include <ostream>
#include <string>

using namespace HInput;
using HW::GamepadAxis;
using HW::GamepadButton;

namespace
{
    HW::RawInput Pad()
    {
        HW::RawInput input;
        input.Gamepad.Connected = true;
        return input;
    }

    void SetAxis(HW::RawInput& input, GamepadAxis axis, float value)
    {
        input.Gamepad.Axes[static_cast<size_t>(axis)] = value;
    }

    void SetButton(HW::RawInput& input, GamepadButton button, bool down)
    {
        input.Gamepad.Buttons[static_cast<size_t>(button)] = down;
    }

    // One action, LeftX with a 0.2 deadzone.
    InputActionMap StickMap(float scale)
    {
        InputActionMap map;
        map.Actions = { { "Steer", { { BindingSource::GamepadAxis, static_cast<uint16_t>(GamepadAxis::LeftX), 0, scale, 0.2f } } } };
        return map;
    }

    float SteerAt(float axis, float scale = 1.0f)
    {
        HW::RawInput input = Pad();
        SetAxis(input, GamepadAxis::LeftX, axis);
        ActionState state;
        UpdateActionState(StickMap(scale), input, state);
        return GetActionValue(state, 0);
    }
}

TEST_CASE("Gamepad - a stick is zero inside its deadzone, rescaled outside it and full at full tilt")
{
    CHECK(SteerAt(0.0f) == 0.0f);
    CHECK(SteerAt(0.2f) == 0.0f);
    CHECK(SteerAt(-0.15f) == 0.0f);
    CHECK(SteerAt(0.6f) == doctest::Approx(0.5f)); // halfway between the deadzone and full tilt
    CHECK(SteerAt(1.0f) == doctest::Approx(1.0f));
    CHECK(SteerAt(-1.0f) == doctest::Approx(-1.0f));
    CHECK(SteerAt(1.0f, -2.0f) == doctest::Approx(-1.0f)); // scaled, then clamped

    // A disconnected pad drives nothing.
    HW::RawInput input = Pad();
    SetAxis(input, GamepadAxis::LeftX, 1.0f);
    input.Gamepad.Connected = false;
    ActionState state;
    UpdateActionState(StickMap(1.0f), input, state);
    CHECK(GetActionValue(state, 0) == 0.0f);
}

TEST_CASE("Gamepad - the stick and the D-pad drive the default UI navigation")
{
    const InputActionSet defaults = MakeDefaultInputActions();
    const size_t         up       = *FindAction(defaults.Game, "UiNavigateUp");
    const size_t         down     = *FindAction(defaults.Game, "UiNavigateDown");
    const size_t         submit   = *FindAction(defaults.Game, "UiSubmit");
    ActionState          state;

    // The stick pushed up (y is down: -1) presses UiNavigateUp once, not UiNavigateDown.
    HW::RawInput input = Pad();
    UpdateActionState(defaults.Game, input, state);
    SetAxis(input, GamepadAxis::LeftY, -0.9f);
    UpdateActionState(defaults.Game, input, state);
    CHECK(WasActionPressed(state, up));
    CHECK_FALSE(IsActionDown(state, down));
    UpdateActionState(defaults.Game, input, state);
    CHECK(IsActionDown(state, up));
    CHECK_FALSE(WasActionPressed(state, up));

    // Half tilt past the deadzone is not yet a press.
    SetAxis(input, GamepadAxis::LeftY, -0.5f);
    UpdateActionState(defaults.Game, input, state);
    CHECK_FALSE(IsActionDown(state, up));

    SetAxis(input, GamepadAxis::LeftY, 0.0f);
    SetButton(input, GamepadButton::DpadUp, true);
    SetButton(input, GamepadButton::A, true);
    UpdateActionState(defaults.Game, input, state);
    CHECK(WasActionPressed(state, up));
    CHECK(WasActionPressed(state, submit));
}

TEST_CASE("Gamepad - bindings round-trip through the actions file, and bad names fail")
{
    const InputActionSet         defaults = MakeDefaultInputActions();
    const std::string            text     = WriteInputActions(defaults);
    CHECK(text.find("- GamepadButton: DpadUp") != std::string::npos);
    CHECK(text.find("- GamepadAxis: LeftY") != std::string::npos);
    CHECK(text.find("Deadzone: 0.2") != std::string::npos);
    const InputActionParseResult parsed = ParseInputActions(text);
    REQUIRE_MESSAGE(parsed.Actions.has_value(), parsed.Error);
    CHECK(*parsed.Actions == defaults);
    CHECK(WriteInputActions(*parsed.Actions) == text);

    const InputActionParseResult badButton = ParseInputActions("Version: 1\nGame:\n  Jump:\n    - GamepadButton: Z\n");
    CHECK(badButton.Error.find("unknown gamepad button 'Z'") != std::string::npos);
    const InputActionParseResult badAxis = ParseInputActions("Version: 1\nGame:\n  Steer:\n    - GamepadAxis: Wheel\n");
    CHECK(badAxis.Error.find("unknown gamepad axis 'Wheel'") != std::string::npos);

    CHECK(GetGamepadButtonNames().size() == HW::GAMEPAD_BUTTON_COUNT);
    CHECK(GetGamepadAxisNames().size() == HW::GAMEPAD_AXIS_COUNT);
    for (const NamedGamepadButton& button : GetGamepadButtonNames())
    {
        CAPTURE(button.Name);
        CHECK(FindGamepadButton(button.Name) == button.Button);
        CHECK(GetGamepadButtonName(static_cast<uint16_t>(button.Button)) == button.Name);
    }
    for (const NamedGamepadAxis& axis : GetGamepadAxisNames())
        CHECK(FindGamepadAxis(axis.Name) == axis.Axis);
}

TEST_CASE("Gamepad - the game sees the pad only while its keyboard is enabled")
{
    HW::RawInput source = Pad();
    SetButton(source, GamepadButton::A, true);

    GameInputRegion region;
    region.Size            = HM::Vector2(100.0f, 100.0f);
    region.PixelSize       = HM::Vector2(100.0f, 100.0f);
    region.KeyboardEnabled = true;
    GameInputGate gate;
    CHECK(MakeGameInput(source, region, gate).Gamepad.Buttons[static_cast<size_t>(GamepadButton::A)]);

    region.KeyboardEnabled = false;
    const HW::RawInput game = MakeGameInput(source, region, gate);
    CHECK_FALSE(game.Gamepad.Connected);
    CHECK_FALSE(game.Gamepad.Buttons[static_cast<size_t>(GamepadButton::A)]);
}

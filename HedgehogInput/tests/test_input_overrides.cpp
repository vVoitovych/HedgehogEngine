#include "HedgehogInput/api/DefaultInputActions.hpp"
#include "HedgehogInput/api/InputActionFile.hpp"
#include "HedgehogInput/api/InputListening.hpp"
#include "HedgehogInput/api/InputOverrides.hpp"

#include "HedgehogEngine/HedgehogWindow/api/InputCodes.hpp"
#include "HedgehogScripting/tests/test_log_capture.hpp"

#include "doctest/doctest/doctest.h"

#include <string>

using namespace HInput;

namespace
{
    InputBinding PadButton(HW::GamepadButton button)
    {
        InputBinding binding;
        binding.Source = BindingSource::GamepadButton;
        binding.Code   = static_cast<uint16_t>(button);
        return binding;
    }

    InputAction MakeAction(const char* name, const InputBinding& binding)
    {
        return InputAction{ name, { binding } };
    }

    const InputAction& Get(const InputActionMap& map, const char* name)
    {
        return map.Actions[*FindAction(map, name)];
    }
}

TEST_CASE("an override replaces one action's bindings and nothing else")
{
    InputActionSet project = MakeDefaultInputActions();
    InputActionMap overrides;
    overrides.Actions.push_back(MakeAction("UiSubmit", PadButton(HW::GamepadButton::X)));
    project.Game.Actions.push_back(MakeAction("Jump", PadButton(HW::GamepadButton::A)));

    const InputActionSet result = ApplyInputOverrides(project, overrides);

    CHECK(Get(result.Game, "UiSubmit").Bindings == overrides.Actions[0].Bindings);
    CHECK(Get(result.Game, "Jump") == Get(project.Game, "Jump"));
    CHECK(Get(result.Game, "UiCancel") == Get(project.Game, "UiCancel"));
    CHECK(result.Editor == project.Editor);
    CHECK(result.Game.Actions.size() == project.Game.Actions.size());
}

TEST_CASE("an override naming no action is dropped with one warning")
{
    const InputActionSet project = MakeDefaultInputActions();
    InputActionMap       overrides;
    overrides.Actions.push_back(MakeAction("NoSuchAction", PadButton(HW::GamepadButton::Y)));
    overrides.Actions.push_back(MakeAction("UiSubmit", PadButton(HW::GamepadButton::X)));

    LogCapture           log;
    const InputActionSet result = ApplyInputOverrides(project, overrides);

    CHECK(log.Lines("NoSuchAction").size() == 1);
    CHECK(Get(result.Game, "UiSubmit").Bindings == overrides.Actions[1].Bindings);
    CHECK(result.Game.Actions.size() == project.Game.Actions.size());
}

TEST_CASE("SetActionBinding and ResetActionBindings edit the overrides")
{
    const InputActionSet project  = MakeDefaultInputActions();
    const InputAction&   original = Get(project.Game, "UiSubmit");
    REQUIRE(original.Bindings.size() > 1);

    InputActionMap overrides;
    CHECK(SetActionBinding(overrides, project.Game, "UiSubmit", 1, PadButton(HW::GamepadButton::X)));
    REQUIRE(overrides.Actions.size() == 1);
    CHECK(overrides.Actions[0].Bindings.size() == original.Bindings.size());
    CHECK(overrides.Actions[0].Bindings[0] == original.Bindings[0]);
    CHECK(overrides.Actions[0].Bindings[1] == PadButton(HW::GamepadButton::X));

    CHECK(SetActionBinding(overrides, project.Game, "UiSubmit", 0, PadButton(HW::GamepadButton::Y)));
    CHECK(overrides.Actions.size() == 1);
    CHECK(overrides.Actions[0].Bindings[0] == PadButton(HW::GamepadButton::Y));
    CHECK(overrides.Actions[0].Bindings[1] == PadButton(HW::GamepadButton::X));

    const InputActionMap before = overrides;
    CHECK_FALSE(SetActionBinding(overrides, project.Game, "NoSuchAction", 0, PadButton(HW::GamepadButton::A)));
    CHECK_FALSE(SetActionBinding(overrides, project.Game, "UiSubmit", original.Bindings.size(),
                                 PadButton(HW::GamepadButton::A)));
    CHECK(overrides == before);

    CHECK(ResetActionBindings(overrides, "UiSubmit"));
    CHECK(overrides.Actions.empty());
    CHECK(ApplyInputOverrides(project, overrides) == project);
    CHECK_FALSE(ResetActionBindings(overrides, "UiSubmit"));
}

TEST_CASE("overrides round trip through the actions file and its Editor section is not used")
{
    InputActionMap overrides;
    overrides.Actions.push_back(MakeAction("UiSubmit", PadButton(HW::GamepadButton::X)));

    InputActionSet file;
    file.Game = overrides;
    const InputActionParseResult read = ParseInputActions(WriteInputActions(file));
    REQUIRE(read.Actions);
    CHECK(read.Actions->Game == overrides);

    const InputActionParseResult withEditor =
        ParseInputActions("Version: 1\nGame:\n  UiSubmit:\n    - GamepadButton: X\nEditor:\n  Fly:\n    - Key: F\n");
    REQUIRE(withEditor.Actions);
    CHECK(withEditor.Actions->Game == overrides);

    const InputActionSet project = MakeDefaultInputActions();
    CHECK(ApplyInputOverrides(project, withEditor.Actions->Game).Editor == project.Editor);
}

TEST_CASE("FindNewPress finds the first newly pressed control of the source's kind")
{
    const HW::RawInput none;

    HW::RawInput keys;
    keys.Keys[static_cast<size_t>(HW::Key::W)] = true;
    keys.Keys[static_cast<size_t>(HW::Key::A)] = true;
    for (const BindingSource source : { BindingSource::Key, BindingSource::KeyAxis })
    {
        const auto press = FindNewPress(none, keys, source);
        REQUIRE(press);
        CHECK(press->Code == static_cast<uint16_t>(HW::Key::A));
    }
    CHECK_FALSE(FindNewPress(keys, keys, BindingSource::Key));

    HW::RawInput mouse;
    mouse.MouseButtons[static_cast<size_t>(HW::MouseButton::Right)] = true;
    REQUIRE(FindNewPress(none, mouse, BindingSource::MouseButton));
    CHECK(FindNewPress(none, mouse, BindingSource::MouseButton)->Code == static_cast<uint16_t>(HW::MouseButton::Right));

    HW::RawInput pad;
    pad.Gamepad.Buttons[static_cast<size_t>(HW::GamepadButton::B)] = true;
    REQUIRE(FindNewPress(none, pad, BindingSource::GamepadButton));
    CHECK(FindNewPress(none, pad, BindingSource::GamepadButton)->Code == static_cast<uint16_t>(HW::GamepadButton::B));
    CHECK_FALSE(FindNewPress(none, pad, BindingSource::Key));
}

TEST_CASE("FindNewPress reads a gamepad axis past the press threshold")
{
    const auto axis = static_cast<size_t>(HW::GamepadAxis::LeftX);
    HW::RawInput low;
    low.Gamepad.Axes[axis] = 0.2f;

    HW::RawInput positive;
    positive.Gamepad.Axes[axis] = 0.6f;
    const auto press = FindNewPress(low, positive, BindingSource::GamepadAxis);
    REQUIRE(press);
    CHECK(press->Code == static_cast<uint16_t>(HW::GamepadAxis::LeftX));
    CHECK(press->Direction == 1.0f);

    HW::RawInput negative;
    negative.Gamepad.Axes[axis] = -0.7f;
    CHECK(FindNewPress(low, negative, BindingSource::GamepadAxis)->Direction == -1.0f);

    HW::RawInput held;
    held.Gamepad.Axes[axis] = 0.8f;
    CHECK_FALSE(FindNewPress(held, held, BindingSource::GamepadAxis));

    HW::RawInput small;
    small.Gamepad.Axes[axis] = 0.4f;
    CHECK_FALSE(FindNewPress(low, small, BindingSource::GamepadAxis));

    HW::RawInput trigger;
    trigger.Gamepad.Axes[static_cast<size_t>(HW::GamepadAxis::RightTrigger)] = 1.0f;
    const auto triggerPress = FindNewPress(HW::RawInput{}, trigger, BindingSource::GamepadAxis);
    REQUIRE(triggerPress);
    CHECK(triggerPress->Direction == 1.0f);
}

TEST_CASE("FindNewPress gives nothing for pointer and scroll sources")
{
    HW::RawInput busy;
    busy.Keys[static_cast<size_t>(HW::Key::W)] = true;
    busy.CursorDelta                           = HM::Vector2(5.0f, 5.0f);
    busy.ScrollDelta                           = HM::Vector2(0.0f, 1.0f);
    for (const BindingSource source : { BindingSource::PointerDeltaX, BindingSource::PointerDeltaY,
                                        BindingSource::ScrollX, BindingSource::ScrollY })
    {
        CHECK_FALSE(FindNewPress(HW::RawInput{}, busy, source));
    }
}

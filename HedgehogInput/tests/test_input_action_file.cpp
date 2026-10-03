#include "doctest/doctest/doctest.h"

#include "HedgehogInput/api/DefaultInputActions.hpp"
#include "HedgehogInput/api/InputActionFile.hpp"
#include "HedgehogInput/api/InputNames.hpp"

#include <ostream>
#include <set>
#include <string>

using namespace HInput;

namespace
{
    // Parses text that must fail, and returns the one error message.
    std::string ErrorOf(const std::string& text)
    {
        const InputActionParseResult result = ParseInputActions(text);
        CHECK_FALSE(result.Actions.has_value());
        CHECK_FALSE(result.Error.empty());
        return result.Error;
    }

    bool Contains(const std::string& text, const std::string& fragment)
    {
        return text.find(fragment) != std::string::npos;
    }
}

TEST_CASE("Input names - every key and mouse button is named once, and every name maps back to its code")
{
    const auto keys = GetKeyNames();
    CHECK(keys.size() == 120); // the HW::Key enumerators
    std::set<uint16_t>         codes;
    std::set<std::string_view> names;
    for (const NamedKey& key : keys)
    {
        CAPTURE(key.Name);
        codes.insert(static_cast<uint16_t>(key.Key));
        names.insert(key.Name);
        CHECK(FindKey(key.Name) == key.Key);
        CHECK(GetKeyName(static_cast<uint16_t>(key.Key)) == key.Name);
    }
    CHECK(codes.size() == keys.size());
    CHECK(names.size() == keys.size());

    const auto buttons = GetMouseButtonNames();
    CHECK(buttons.size() == HW::MOUSE_BUTTON_COUNT);
    for (const NamedMouseButton& button : buttons)
    {
        CAPTURE(button.Name);
        CHECK(FindMouseButton(button.Name) == button.Button);
        CHECK(GetMouseButtonName(static_cast<uint16_t>(button.Button)) == button.Name);
    }

    CHECK(FindKey("Enter") == HW::Key::Enter);
    CHECK(FindKey("KeypadEnter") == HW::Key::KeypadEnter);
    CHECK_FALSE(FindKey("enter").has_value()); // case-sensitive
    CHECK_FALSE(FindKey("Return").has_value());
    CHECK(GetKeyName(0).empty());
    CHECK(GetMouseButtonName(99).empty());
}

TEST_CASE("Action file - the defaults write, read back equal, and write again byte for byte")
{
    const InputActionSet defaults = MakeDefaultInputActions();
    const std::string    text     = WriteInputActions(defaults);
    CHECK(Contains(text, "Version: 1"));
    CHECK(Contains(text, "- Key: Enter"));
    CHECK(Contains(text, "KeyAxis: {Negative: S, Positive: W}"));
    CHECK(Contains(text, "PointerDelta: X"));
    CHECK(Contains(text, "Deadzone: 2"));
    CHECK_FALSE(Contains(text, "Scale")); // the default scale is left out

    const InputActionParseResult parsed = ParseInputActions(text);
    REQUIRE_MESSAGE(parsed.Actions.has_value(), parsed.Error);
    CHECK(*parsed.Actions == defaults);
    CHECK(WriteInputActions(*parsed.Actions) == text);
}

TEST_CASE("Action file - every source reads, with scale and deadzone, keeping action and binding order")
{
    const std::string text = R"(Version: 1
Game:
  Zoom:
    - Scroll: Y
      Scale: -2.5
  Fire:
    - MouseButton: Right
    - Key: LeftControl
  MoveX:
    - KeyAxis: { Negative: A, Positive: D }
  Look:
    - PointerDelta: Y
      Deadzone: 1.5
  Nothing: []
)";
    const InputActionParseResult parsed = ParseInputActions(text);
    REQUIRE_MESSAGE(parsed.Actions.has_value(), parsed.Error);
    const InputActionMap& game = parsed.Actions->Game;
    CHECK(parsed.Actions->Editor.Actions.empty()); // a missing section is an empty map
    REQUIRE(game.Actions.size() == 5);
    CHECK(game.Actions[0].Name == "Zoom");
    CHECK(game.Actions[1].Name == "Fire");
    CHECK(game.Actions[4].Bindings.empty());

    const InputBinding& zoom = game.Actions[0].Bindings[0];
    CHECK(zoom.Source == BindingSource::ScrollY);
    CHECK(zoom.Scale == -2.5f);

    REQUIRE(game.Actions[1].Bindings.size() == 2);
    CHECK(game.Actions[1].Bindings[0].Source == BindingSource::MouseButton);
    CHECK(game.Actions[1].Bindings[0].Code == static_cast<uint16_t>(HW::MouseButton::Right));
    CHECK(game.Actions[1].Bindings[1].Code == static_cast<uint16_t>(HW::Key::LeftControl));

    const InputBinding& move = game.Actions[2].Bindings[0];
    CHECK(move.Source == BindingSource::KeyAxis);
    CHECK(move.NegativeCode == static_cast<uint16_t>(HW::Key::A));
    CHECK(move.Code == static_cast<uint16_t>(HW::Key::D));

    const InputBinding& look = game.Actions[3].Bindings[0];
    CHECK(look.Source == BindingSource::PointerDeltaY);
    CHECK(look.Deadzone == 1.5f);

    // And it survives a write and a read.
    const InputActionParseResult again = ParseInputActions(WriteInputActions(*parsed.Actions));
    REQUIRE(again.Actions.has_value());
    CHECK(*again.Actions == *parsed.Actions);
}

TEST_CASE("Action file - each kind of mistake fails with one message naming what is wrong")
{
    CHECK(Contains(ErrorOf("Version: 1\nGame:\n  Jump:\n    - Key: Spcae\n"), "Game: action 'Jump', binding 0: unknown key 'Spcae'"));
    CHECK(Contains(ErrorOf("Version: 1\nGame:\n  Jump:\n    - Key: Space\n    - Button: Left\n"),
                   "binding 1: unknown field 'Button'"));
    CHECK(Contains(ErrorOf("Version: 1\nEditor:\n  Look:\n    - Scale: 2\n"), "Editor: action 'Look', binding 0: no source"));
    CHECK(Contains(ErrorOf("Version: 1\nGame:\n  Jump:\n    - Key: Space\n      MouseButton: Left\n"), "more than one source"));
    CHECK(Contains(ErrorOf("Version: 1\nGame:\n  Jump:\n    - MouseButton: Thumb\n"), "unknown mouse button 'Thumb'"));
    CHECK(Contains(ErrorOf("Version: 1\nGame:\n  Look:\n    - PointerDelta: Z\n"), "axis 'Z' is not X or Y"));
    CHECK(Contains(ErrorOf("Version: 1\nGame:\n  Move:\n    - KeyAxis: { Negative: A }\n"), "needs Negative and Positive"));
    CHECK(Contains(ErrorOf("Version: 1\nGame:\n  Jump:\n    - Key: Space\n      Scale: big\n"), "'big' is not a number"));
    CHECK(Contains(ErrorOf("Version: 1\nGame:\n  Jump:\n    - Key: Space\n  Jump:\n    - Key: W\n"),
                   "map keys must be unique")); // yaml-cpp's own check, with the line
    CHECK(Contains(ErrorOf("Version: 1\nPlayer:\n  Jump: []\n"), "unknown top-level key 'Player'"));
    CHECK(Contains(ErrorOf("Game:\n  Jump: []\n"), "Version is missing"));
    CHECK(Contains(ErrorOf("Version: 2\n"), "Version 2 is not supported"));
    CHECK(Contains(ErrorOf("Version: 1\nGame: [unclosed\n"), "not valid YAML"));
    CHECK(Contains(ErrorOf("Version: 1\nGame:\n  Jump: Space\n"), "expected a list of bindings"));
}

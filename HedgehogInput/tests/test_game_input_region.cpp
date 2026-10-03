#include "doctest/doctest/doctest.h"

#include "HedgehogInput/api/GameInputRegion.hpp"

using namespace HInput;

namespace
{
    constexpr size_t LEFT = static_cast<size_t>(HW::MouseButton::Left);

    // A 400x300 game view at (100, 50) of the window, rendering 800x600 pixels: twice the scale.
    GameInputRegion Region()
    {
        GameInputRegion region;
        region.Origin          = HM::Vector2(100.0f, 50.0f);
        region.Size            = HM::Vector2(400.0f, 300.0f);
        region.PixelSize       = HM::Vector2(800.0f, 600.0f);
        region.PointerEnabled  = true;
        region.KeyboardEnabled = true;
        return region;
    }

    HW::RawInput CursorAt(float x, float y)
    {
        HW::RawInput input;
        input.CursorPosition = HM::Vector2(x, y);
        input.CursorKnown    = true;
        input.CursorInside   = true;
        return input;
    }
}

TEST_CASE("Game input - the cursor maps into the game view's pixels")
{
    GameInputGate gate;
    HW::RawInput  source = CursorAt(100.0f, 50.0f);
    source.CursorDelta   = HM::Vector2(3.0f, -2.0f);

    HW::RawInput game = MakeGameInput(source, Region(), gate);
    CHECK(game.CursorPosition == HM::Vector2(0.0f, 0.0f));
    CHECK(game.CursorDelta == HM::Vector2(6.0f, -4.0f)); // twice the scale
    CHECK(game.CursorInside);

    source.CursorPosition = HM::Vector2(499.0f, 349.0f);
    game                  = MakeGameInput(source, Region(), gate);
    CHECK(game.CursorPosition == HM::Vector2(798.0f, 598.0f));
    CHECK(game.CursorInside);

    // The far edge is outside; so is anywhere with the pointer disabled.
    source.CursorPosition = HM::Vector2(500.0f, 100.0f);
    CHECK_FALSE(MakeGameInput(source, Region(), gate).CursorInside);
    GameInputRegion disabled = Region();
    disabled.PointerEnabled  = false;
    source.CursorPosition    = HM::Vector2(200.0f, 100.0f);
    CHECK_FALSE(MakeGameInput(source, disabled, gate).CursorInside);
}

TEST_CASE("Game input - a press must begin inside, and is held until its release wherever the cursor goes")
{
    GameInputGate gate;

    // A press outside never reaches the game, even when the cursor then moves in.
    HW::RawInput outside        = CursorAt(20.0f, 20.0f);
    outside.MouseButtons[LEFT] = true;
    CHECK_FALSE(MakeGameInput(outside, Region(), gate).MouseButtons[LEFT]);
    HW::RawInput movedIn        = CursorAt(200.0f, 100.0f);
    movedIn.MouseButtons[LEFT] = true;
    CHECK_FALSE(MakeGameInput(movedIn, Region(), gate).MouseButtons[LEFT]);

    // Released, then pressed inside: the game holds it, also after the cursor leaves.
    CHECK_FALSE(MakeGameInput(CursorAt(200.0f, 100.0f), Region(), gate).MouseButtons[LEFT]);
    CHECK(MakeGameInput(movedIn, Region(), gate).MouseButtons[LEFT]);
    HW::RawInput dragged        = CursorAt(900.0f, 700.0f);
    dragged.MouseButtons[LEFT] = true;
    const HW::RawInput game    = MakeGameInput(dragged, Region(), gate);
    CHECK(game.MouseButtons[LEFT]);
    CHECK_FALSE(game.CursorInside);

    // The release reaches the game outside too.
    CHECK_FALSE(MakeGameInput(CursorAt(900.0f, 700.0f), Region(), gate).MouseButtons[LEFT]);
}

TEST_CASE("Game input - scroll only inside, keys only with the keyboard enabled")
{
    GameInputGate gate;
    HW::RawInput  source                               = CursorAt(200.0f, 100.0f);
    source.ScrollDelta                                 = HM::Vector2(0.0f, 2.0f);
    source.Keys[static_cast<size_t>(HW::Key::Enter)] = true;

    HW::RawInput game = MakeGameInput(source, Region(), gate);
    CHECK(game.ScrollDelta == HM::Vector2(0.0f, 2.0f));
    CHECK(HW::IsKeyDown(game, HW::Key::Enter));

    GameInputRegion noKeyboard = Region();
    noKeyboard.KeyboardEnabled = false;
    CHECK_FALSE(HW::IsKeyDown(MakeGameInput(source, noKeyboard, gate), HW::Key::Enter));

    source.CursorPosition = HM::Vector2(20.0f, 20.0f);
    CHECK(MakeGameInput(source, Region(), gate).ScrollDelta == HM::Vector2(0.0f, 0.0f));
}

TEST_CASE("Game input - a region with no area gives no input and forgets held buttons")
{
    GameInputGate gate;
    HW::RawInput  source                               = CursorAt(200.0f, 100.0f);
    source.MouseButtons[LEFT]                          = true;
    source.Keys[static_cast<size_t>(HW::Key::Enter)] = true;
    REQUIRE(MakeGameInput(source, Region(), gate).MouseButtons[LEFT]);

    GameInputRegion hidden = Region();
    hidden.Size            = HM::Vector2(0.0f, 0.0f);
    const HW::RawInput game = MakeGameInput(source, hidden, gate);
    CHECK_FALSE(game.MouseButtons[LEFT]);
    CHECK_FALSE(HW::IsKeyDown(game, HW::Key::Enter));
    CHECK_FALSE(game.CursorInside);
    CHECK_FALSE(gate.Held[LEFT]);
}

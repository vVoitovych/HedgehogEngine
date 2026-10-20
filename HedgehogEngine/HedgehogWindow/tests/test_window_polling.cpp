#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/HedgehogWindow/api/RawInputEvents.hpp"
#include "HedgehogEngine/HedgehogWindow/api/WindowPolling.hpp"

using namespace HW;

namespace
{
    PolledWindowState FocusedAt(double x, double y)
    {
        PolledWindowState state;
        state.Focused      = true;
        state.CursorInside = true;
        state.CursorX      = x;
        state.CursorY      = y;
        return state;
    }

    // One frame of polling, as the editor does it.
    void Frame(RawInput& input, const PolledWindowState& state)
    {
        BeginInputFrame(input);
        ApplyPolledWindowState(input, state);
    }
}

TEST_CASE("WindowPolling - a polled key and button go down and up with the window's state")
{
    RawInput          input;
    PolledWindowState state = FocusedAt(10.0, 20.0);
    state.Keys[static_cast<size_t>(Key::W)]                    = true;
    state.MouseButtons[static_cast<size_t>(MouseButton::Right)] = true;
    Frame(input, state);
    CHECK(IsKeyDown(input, Key::W));
    CHECK(IsMouseButtonDown(input, MouseButton::Right));
    CHECK(input.Focused);
    CHECK(input.CursorInside);

    state.Keys[static_cast<size_t>(Key::W)]                    = false;
    state.MouseButtons[static_cast<size_t>(MouseButton::Right)] = false;
    Frame(input, state);
    CHECK_FALSE(IsKeyDown(input, Key::W));
    CHECK_FALSE(IsMouseButtonDown(input, MouseButton::Right));
}

TEST_CASE("WindowPolling - the cursor gives a delta from its second poll on, and none after it left")
{
    RawInput input;
    Frame(input, FocusedAt(100.0, 50.0));
    CHECK(input.CursorPosition == HM::Vector2(100.0f, 50.0f));
    CHECK(input.CursorDelta == HM::Vector2(0.0f, 0.0f)); // the first position is no move

    Frame(input, FocusedAt(110.0, 45.0));
    CHECK(input.CursorDelta == HM::Vector2(10.0f, -5.0f));

    PolledWindowState outside = FocusedAt(400.0, 400.0);
    outside.CursorInside      = false;
    Frame(input, outside);
    CHECK_FALSE(input.CursorInside);
    CHECK(input.CursorDelta == HM::Vector2(0.0f, 0.0f)); // not followed outside without a button

    Frame(input, FocusedAt(300.0, 300.0));
    CHECK(input.CursorDelta == HM::Vector2(0.0f, 0.0f)); // coming back is not a jump
}

TEST_CASE("WindowPolling - a drag that leaves the window keeps moving the cursor")
{
    RawInput          input;
    PolledWindowState state = FocusedAt(10.0, 10.0);
    state.MouseButtons[static_cast<size_t>(MouseButton::Left)] = true;
    Frame(input, state);

    state.CursorInside = false;
    state.CursorX      = -40.0;
    Frame(input, state);
    CHECK(IsMouseButtonDown(input, MouseButton::Left));
    CHECK(input.CursorDelta == HM::Vector2(-50.0f, 0.0f));
}

TEST_CASE("WindowPolling - an unfocused window passes no key, and losing focus releases them")
{
    RawInput          input;
    PolledWindowState state = FocusedAt(0.0, 0.0);
    state.Keys[static_cast<size_t>(Key::A)] = true;
    Frame(input, state);
    REQUIRE(IsKeyDown(input, Key::A));

    state.Focused = false; // GLFW still reports the key, now held for another window
    Frame(input, state);
    CHECK_FALSE(input.Focused);
    CHECK_FALSE(IsKeyDown(input, Key::A));
}

TEST_CASE("WindowPolling - polling no window gives an empty state")
{
    const PolledWindowState state = PollWindowState(nullptr);
    CHECK_FALSE(state.Focused);
    CHECK_FALSE(state.CursorInside);
    for (const bool down : state.Keys)
        CHECK_FALSE(down);
}

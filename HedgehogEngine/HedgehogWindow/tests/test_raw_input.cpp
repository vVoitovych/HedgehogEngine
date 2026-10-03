#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/HedgehogWindow/api/RawInputEvents.hpp"

using namespace HW;

namespace
{
    // GLFW's actions as the window passes them on: a press and a repeat are both "down".
    constexpr bool DOWN = true;
    constexpr bool UP   = false;

    int Code(Key key)
    {
        return static_cast<int>(key);
    }

    size_t CountDown(const RawInput& input)
    {
        size_t count = 0;
        for (const bool down : input.Keys)
            count += down ? 1 : 0;
        for (const bool down : input.MouseButtons)
            count += down ? 1 : 0;
        return count;
    }
}

TEST_CASE("RawInput - a key goes down and up, and a repeat keeps it down")
{
    RawInput input;
    ApplyKeyEvent(input, Code(Key::W), DOWN);
    CHECK(IsKeyDown(input, Key::W));
    CHECK(CountDown(input) == 1);

    ApplyKeyEvent(input, Code(Key::W), DOWN); // a repeat
    CHECK(IsKeyDown(input, Key::W));

    ApplyKeyEvent(input, Code(Key::Enter), DOWN);
    ApplyKeyEvent(input, Code(Key::W), UP);
    CHECK_FALSE(IsKeyDown(input, Key::W));
    CHECK(IsKeyDown(input, Key::Enter));
    CHECK(CountDown(input) == 1);

    // The ends of the code range.
    ApplyKeyEvent(input, Code(Key::Space), DOWN);
    ApplyKeyEvent(input, Code(Key::Menu), DOWN);
    CHECK(IsKeyDown(input, Key::Space));
    CHECK(IsKeyDown(input, Key::Menu));
}

TEST_CASE("RawInput - an unknown or out-of-range key changes nothing")
{
    RawInput input;
    ApplyKeyEvent(input, -1, DOWN); // GLFW_KEY_UNKNOWN
    ApplyKeyEvent(input, static_cast<int>(KEY_COUNT), DOWN);
    ApplyKeyEvent(input, 100000, DOWN);
    CHECK(CountDown(input) == 0);

    ApplyMouseButtonEvent(input, -1, DOWN);
    ApplyMouseButtonEvent(input, static_cast<int>(MOUSE_BUTTON_COUNT), DOWN);
    CHECK(CountDown(input) == 0);
}

TEST_CASE("RawInput - all eight mouse buttons go down and up on their own")
{
    RawInput input;
    for (size_t button = 0; button < MOUSE_BUTTON_COUNT; ++button)
    {
        CAPTURE(button);
        ApplyMouseButtonEvent(input, static_cast<int>(button), DOWN);
        CHECK(IsMouseButtonDown(input, static_cast<MouseButton>(button)));
        CHECK(CountDown(input) == 1);
        ApplyMouseButtonEvent(input, static_cast<int>(button), UP);
        CHECK(CountDown(input) == 0);
    }
}

TEST_CASE("RawInput - cursor moves add up within a frame, and the first position is no jump")
{
    RawInput input;
    ApplyCursorEnter(input, true);
    CHECK(input.CursorInside);

    // The first event only places the cursor.
    ApplyCursorEvent(input, 100.0, 50.0);
    CHECK(input.CursorPosition == HM::Vector2(100.0f, 50.0f));
    CHECK(input.CursorDelta == HM::Vector2(0.0f, 0.0f));

    ApplyCursorEvent(input, 110.0, 45.0);
    ApplyCursorEvent(input, 115.0, 40.0);
    CHECK(input.CursorPosition == HM::Vector2(115.0f, 40.0f));
    CHECK(input.CursorDelta == HM::Vector2(15.0f, -10.0f));

    // A new frame starts from no movement, and still knows where the cursor is.
    BeginInputFrame(input);
    CHECK(input.CursorDelta == HM::Vector2(0.0f, 0.0f));
    ApplyCursorEvent(input, 116.0, 40.0);
    CHECK(input.CursorDelta == HM::Vector2(1.0f, 0.0f));

    // Leaving and coming back on the far side is not a jump.
    BeginInputFrame(input);
    ApplyCursorEnter(input, false);
    CHECK_FALSE(input.CursorInside);
    ApplyCursorEnter(input, true);
    ApplyCursorEvent(input, 900.0, 600.0);
    CHECK(input.CursorDelta == HM::Vector2(0.0f, 0.0f));
    CHECK(input.CursorPosition == HM::Vector2(900.0f, 600.0f));
}

TEST_CASE("RawInput - scrolls within a frame add up, and a new frame starts from none")
{
    RawInput input;
    ApplyScrollEvent(input, 0.0, 1.0);
    ApplyScrollEvent(input, 0.5, 2.0);
    CHECK(input.ScrollDelta == HM::Vector2(0.5f, 3.0f));
    BeginInputFrame(input);
    CHECK(input.ScrollDelta == HM::Vector2(0.0f, 0.0f));
}

TEST_CASE("RawInput - losing focus releases every key and mouse button")
{
    RawInput input;
    ApplyKeyEvent(input, Code(Key::LeftShift), DOWN);
    ApplyKeyEvent(input, Code(Key::A), DOWN);
    ApplyMouseButtonEvent(input, static_cast<int>(MouseButton::Right), DOWN);
    REQUIRE(CountDown(input) == 3);

    ApplyFocus(input, false);
    CHECK_FALSE(input.Focused);
    CHECK(CountDown(input) == 0);

    // Regaining focus presses nothing; the next press does.
    ApplyFocus(input, true);
    CHECK(input.Focused);
    CHECK(CountDown(input) == 0);
    ApplyKeyEvent(input, Code(Key::A), DOWN);
    CHECK(IsKeyDown(input, Key::A));
}

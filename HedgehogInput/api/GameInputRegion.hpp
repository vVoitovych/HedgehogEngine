#pragma once

#include "HedgehogEngine/HedgehogWindow/api/RawInput.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <array>

// The part of a window's input that belongs to the game: the game view's rectangle, with the cursor
// mapped into the game view's pixels, and the keys and buttons the game is allowed to see.
namespace HInput
{
    struct GameInputRegion
    {
        HM::Vector2 Origin    = HM::Vector2(0.0f, 0.0f); // the game view's top-left, in the raw input's coordinates
        HM::Vector2 Size      = HM::Vector2(0.0f, 0.0f); // its size, in the same coordinates
        HM::Vector2 PixelSize = HM::Vector2(0.0f, 0.0f); // its size in the game view's own pixels
        bool        PointerEnabled  = false;             // the pointer may press, release and scroll in it
        bool        KeyboardEnabled = false;             // keys and the gamepad reach the game
    };

    // What MakeGameInput remembers between frames, per mouse button: whether the source had it down
    // last frame, and whether the game holds it because its press began inside the region.
    struct GameInputGate
    {
        std::array<bool, HW::MOUSE_BUTTON_COUNT> SourceDown{};
        std::array<bool, HW::MOUSE_BUTTON_COUNT> Held{};
    };

    // The game's view of source:
    // - the cursor's position and move in the region's pixels ((position - Origin) * PixelSize / Size),
    //   CursorInside while the pointer is enabled and inside the region;
    // - a mouse button the game holds from a press that began inside until its release, wherever the
    //   cursor goes meanwhile, so presses and releases stay paired; a press that began outside, or
    //   while the pointer was disabled, never reaches the game;
    // - scroll only while inside, keys and the gamepad only while the keyboard is enabled.
    // A region with no area gives no input at all. gate carries the button state between frames.
    [[nodiscard]] HW::RawInput MakeGameInput(const HW::RawInput& source, const GameInputRegion& region, GameInputGate& gate);
}

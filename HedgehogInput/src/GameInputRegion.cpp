#include "HedgehogInput/api/GameInputRegion.hpp"

namespace HInput
{
    HW::RawInput MakeGameInput(const HW::RawInput& source, const GameInputRegion& region, GameInputGate& gate)
    {
        HW::RawInput game;
        game.Focused = source.Focused;
        if (!(region.Size.x() > 0.0f && region.Size.y() > 0.0f && region.PixelSize.x() > 0.0f && region.PixelSize.y() > 0.0f))
        {
            gate = GameInputGate{};
            return game;
        }

        const HM::Vector2 scale(region.PixelSize.x() / region.Size.x(), region.PixelSize.y() / region.Size.y());
        const HM::Vector2 position = source.CursorPosition - region.Origin;
        game.CursorPosition = HM::Vector2(position.x() * scale.x(), position.y() * scale.y());
        game.CursorDelta    = HM::Vector2(source.CursorDelta.x() * scale.x(), source.CursorDelta.y() * scale.y());
        game.CursorKnown    = source.CursorKnown;
        game.CursorInside   = region.PointerEnabled && source.CursorInside && game.CursorPosition.x() >= 0.0f &&
                            game.CursorPosition.y() >= 0.0f && game.CursorPosition.x() < region.PixelSize.x() &&
                            game.CursorPosition.y() < region.PixelSize.y();

        for (size_t button = 0; button < HW::MOUSE_BUTTON_COUNT; ++button)
        {
            const bool down     = source.MouseButtons[button];
            const bool newPress = down && !gate.SourceDown[button];
            if (!down)
                gate.Held[button] = false;
            else if (newPress && game.CursorInside)
                gate.Held[button] = true;
            gate.SourceDown[button]   = down;
            game.MouseButtons[button] = gate.Held[button];
        }

        if (game.CursorInside)
            game.ScrollDelta = source.ScrollDelta;
        if (region.KeyboardEnabled)
            game.Keys = source.Keys;
        return game;
    }
}

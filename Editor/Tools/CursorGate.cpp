#include "CursorGate.hpp"

namespace Editor
{
    HW::CursorMode ResolveEditorCursor(HW::CursorMode requested, bool playing, bool gameFocused, bool suspended)
    {
        return (playing && gameFocused && !suspended) ? requested : HW::CursorMode::Normal;
    }

    bool UpdateCursorSuspension(bool suspended, bool playing, bool escapeDown, bool windowFocused,
                                bool gameImagePressed)
    {
        if (!playing)
        {
            return false;
        }
        if (escapeDown || !windowFocused)
        {
            return true;
        }
        return gameImagePressed ? false : suspended;
    }

    // A held Escape keeps the cursor suspended, so the applied cursor is Normal for the rest of the hold:
    // a hold that began under Normal never sees a non-Normal applied cursor, and needs no state of its own.
    bool UpdateEscapeCapture(bool captured, bool escapeDown, HW::CursorMode appliedLastFrame)
    {
        return escapeDown && (captured || appliedLastFrame != HW::CursorMode::Normal);
    }
}

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
}

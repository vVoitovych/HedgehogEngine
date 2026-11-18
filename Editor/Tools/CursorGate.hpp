#pragma once

#include "HedgehogEngine/HedgehogWindow/api/CursorMode.hpp"

namespace Editor
{
    // The cursor the editor applies: the script's request only while Playing, the Game view focused
    // and not suspended, else Normal. ImGui-free, so EditorTest checks it.
    [[nodiscard]] HW::CursorMode ResolveEditorCursor(HW::CursorMode requested, bool playing, bool gameFocused,
                                                     bool suspended);

    // Escape or the Game view's window losing focus suspends the script's cursor; a press on the Game
    // image clears it (Escape wins a tie); leaving Play clears it, so the next Play starts fresh.
    [[nodiscard]] bool UpdateCursorSuspension(bool suspended, bool playing, bool escapeDown, bool windowFocused,
                                              bool gameImagePressed);

    // Escape belongs to the editor for the press that releases an applied cursor, while that press is held;
    // a later press (the cursor already Normal) reaches the game.
    [[nodiscard]] bool UpdateEscapeCapture(bool captured, bool escapeDown, HW::CursorMode appliedLastFrame);
}

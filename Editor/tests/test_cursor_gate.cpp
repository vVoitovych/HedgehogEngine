#include "doctest/doctest/doctest.h"

#include "Tools/CursorGate.hpp"

#include <initializer_list>

using HW::CursorMode;

TEST_CASE("ResolveEditorCursor gives the request only while playing, focused and not suspended")
{
    for (const CursorMode mode : { CursorMode::Hidden, CursorMode::Locked, CursorMode::Confined })
    {
        CHECK(Editor::ResolveEditorCursor(mode, true, true, false) == mode);
        CHECK(Editor::ResolveEditorCursor(mode, false, true, false) == CursorMode::Normal);
        CHECK(Editor::ResolveEditorCursor(mode, true, false, false) == CursorMode::Normal);
        CHECK(Editor::ResolveEditorCursor(mode, true, true, true) == CursorMode::Normal);
    }
    CHECK(Editor::ResolveEditorCursor(CursorMode::Normal, true, true, false) == CursorMode::Normal);
}

TEST_CASE("UpdateCursorSuspension")
{
    CHECK(Editor::UpdateCursorSuspension(false, true, true, true, false));
    CHECK(Editor::UpdateCursorSuspension(false, true, false, false, false));
    CHECK_FALSE(Editor::UpdateCursorSuspension(true, true, false, true, true));
    CHECK(Editor::UpdateCursorSuspension(false, true, true, true, true));
    CHECK_FALSE(Editor::UpdateCursorSuspension(true, false, false, true, false));
    CHECK(Editor::UpdateCursorSuspension(true, true, false, true, false));
    CHECK_FALSE(Editor::UpdateCursorSuspension(false, true, false, true, false));
}

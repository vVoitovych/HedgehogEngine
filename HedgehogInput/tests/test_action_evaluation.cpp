#include "doctest/doctest/doctest.h"

#include "HedgehogInput/api/ActionState.hpp"
#include "HedgehogInput/api/DefaultInputActions.hpp"

#include <atomic>
#include <cstdlib>
#include <new>

using namespace HInput;
using HW::Key;
using HW::MouseButton;

// Counts global allocations while s_Counting is set, so a test can prove a span of code allocates
// nothing. This test executable owns the global operator new.
namespace
{
    std::atomic<bool>   s_Counting    = false;
    std::atomic<size_t> s_Allocations = 0;
}

void* operator new(size_t size)
{
    if (s_Counting)
        ++s_Allocations;
    if (void* memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, size_t) noexcept
{
    std::free(memory);
}

namespace
{
    InputBinding KeyBinding(Key key)
    {
        return { BindingSource::Key, static_cast<uint16_t>(key) };
    }

    void SetKey(HW::RawInput& input, Key key, bool down)
    {
        input.Keys[static_cast<size_t>(key)] = down;
    }

    // Actions: 0 Jump (Space, or W), 1 Move (A/D key axis), 2 LookX (pointer x, 2 px deadzone,
    // scale 0.5), 3 Zoom (scroll y), 4 Fire (left mouse button).
    InputActionMap TestMap()
    {
        InputBinding look;
        look.Source   = BindingSource::PointerDeltaX;
        look.Scale    = 0.5f;
        look.Deadzone = 2.0f;

        InputBinding zoom;
        zoom.Source = BindingSource::ScrollY;
        zoom.Scale  = 2.0f;

        InputActionMap map;
        map.Actions = {
            { "Jump", { KeyBinding(Key::Space), KeyBinding(Key::W) } },
            { "Move", { { BindingSource::KeyAxis, static_cast<uint16_t>(Key::D), static_cast<uint16_t>(Key::A) } } },
            { "LookX", { look } },
            { "Zoom", { zoom } },
            { "Fire", { { BindingSource::MouseButton, static_cast<uint16_t>(MouseButton::Left) } } },
        };
        return map;
    }

    constexpr size_t JUMP = 0, MOVE = 1, LOOK = 2, ZOOM = 3, FIRE = 4;
}

TEST_CASE("Actions - a held key is pressed once, stays down, and is released once")
{
    const InputActionMap map = TestMap();
    HW::RawInput         input;
    ActionState          state;

    UpdateActionState(map, input, state);
    REQUIRE(state.Actions.size() == map.Actions.size());
    CHECK_FALSE(IsActionDown(state, JUMP));

    SetKey(input, Key::Space, true);
    UpdateActionState(map, input, state);
    CHECK(WasActionPressed(state, JUMP));
    CHECK(IsActionDown(state, JUMP));
    CHECK(GetActionValue(state, JUMP) == 1.0f);

    for (int frame = 0; frame < 2; ++frame)
    {
        UpdateActionState(map, input, state);
        CHECK_FALSE(WasActionPressed(state, JUMP));
        CHECK(IsActionDown(state, JUMP));
    }

    SetKey(input, Key::Space, false);
    UpdateActionState(map, input, state);
    CHECK(WasActionReleased(state, JUMP));
    CHECK_FALSE(IsActionDown(state, JUMP));
    UpdateActionState(map, input, state);
    CHECK_FALSE(WasActionReleased(state, JUMP));
}

TEST_CASE("Actions - two bindings on one action: releasing one keeps it down")
{
    const InputActionMap map = TestMap();
    HW::RawInput         input;
    ActionState          state;

    SetKey(input, Key::Space, true);
    SetKey(input, Key::W, true);
    UpdateActionState(map, input, state);
    SetKey(input, Key::Space, false);
    UpdateActionState(map, input, state);
    CHECK(IsActionDown(state, JUMP));
    CHECK_FALSE(WasActionReleased(state, JUMP));
    CHECK_FALSE(WasActionPressed(state, JUMP));
}

TEST_CASE("Actions - a key axis gives -1, 0 and +1, and both keys cancel")
{
    const InputActionMap map = TestMap();
    HW::RawInput         input;
    ActionState          state;

    UpdateActionState(map, input, state);
    CHECK(GetActionValue(state, MOVE) == 0.0f);

    SetKey(input, Key::D, true);
    UpdateActionState(map, input, state);
    CHECK(GetActionValue(state, MOVE) == 1.0f);
    CHECK(IsActionDown(state, MOVE));

    SetKey(input, Key::A, true);
    UpdateActionState(map, input, state);
    CHECK(GetActionValue(state, MOVE) == 0.0f);

    SetKey(input, Key::D, false);
    UpdateActionState(map, input, state);
    CHECK(GetActionValue(state, MOVE) == -1.0f);
    CHECK_FALSE(IsActionDown(state, MOVE)); // down means a positive value
}

TEST_CASE("Actions - pointer moves within the deadzone count as none, larger ones are scaled; scroll is scaled")
{
    const InputActionMap map = TestMap();
    HW::RawInput         input;
    ActionState          state;

    input.CursorDelta = HM::Vector2(2.0f, 0.0f);
    UpdateActionState(map, input, state);
    CHECK(GetActionValue(state, LOOK) == 0.0f);

    input.CursorDelta = HM::Vector2(-10.0f, 3.0f);
    UpdateActionState(map, input, state);
    CHECK(GetActionValue(state, LOOK) == -5.0f); // unclamped pixels times the scale

    input.ScrollDelta = HM::Vector2(0.0f, 1.5f);
    UpdateActionState(map, input, state);
    CHECK(GetActionValue(state, ZOOM) == 3.0f);
    CHECK(IsActionDown(state, ZOOM));
}

TEST_CASE("Actions - a mouse button drives its action, and the pointer follows the cursor")
{
    const InputActionMap map = TestMap();
    HW::RawInput         input;
    ActionState          state;

    input.MouseButtons[static_cast<size_t>(MouseButton::Left)] = true;
    input.CursorPosition = HM::Vector2(320.0f, 240.0f);
    input.CursorDelta    = HM::Vector2(4.0f, -1.0f);
    input.CursorInside   = true;
    UpdateActionState(map, input, state);
    CHECK(WasActionPressed(state, FIRE));
    CHECK(state.Pointer.Position == HM::Vector2(320.0f, 240.0f));
    CHECK(state.Pointer.Delta == HM::Vector2(4.0f, -1.0f));
    CHECK(state.Pointer.Inside);
}

TEST_CASE("Actions - a consumed action reads as up for the rest of the frame only")
{
    const InputActionMap map = TestMap();
    HW::RawInput         input;
    ActionState          state;

    SetKey(input, Key::Space, true);
    SetKey(input, Key::D, true);
    UpdateActionState(map, input, state);
    ConsumeAction(state, JUMP);
    CHECK_FALSE(WasActionPressed(state, JUMP));
    CHECK_FALSE(IsActionDown(state, JUMP));
    CHECK(GetActionValue(state, JUMP) == 0.0f);
    CHECK(IsActionDown(state, MOVE)); // the others are untouched
    ConsumeAction(state, 99);        // out of range: nothing

    UpdateActionState(map, input, state);
    CHECK(IsActionDown(state, JUMP));
    CHECK_FALSE(WasActionPressed(state, JUMP)); // the press was this frame's, not the next one's
}

TEST_CASE("Actions - a state sized for another map is reset rather than giving false edges")
{
    const InputActionMap map = TestMap();
    HW::RawInput         input;
    SetKey(input, Key::Space, true);

    ActionState state;
    state.Actions.assign(2, ActionValue{ 1.0f, true, false, false, false });
    UpdateActionState(map, input, state);
    REQUIRE(state.Actions.size() == map.Actions.size());
    CHECK(WasActionPressed(state, JUMP)); // from up, as after a reset
    CHECK_FALSE(WasActionReleased(state, MOVE));

    ResetActionState(state, map);
    CHECK_FALSE(IsActionDown(state, JUMP));
    CHECK_FALSE(state.Pointer.Inside);
    CHECK_FALSE(IsActionDown(state, 99));
    CHECK(GetActionValue(state, 99) == 0.0f);
}

TEST_CASE("Actions - FindAction matches the exact name")
{
    const InputActionMap map = TestMap();
    CHECK(FindAction(map, "Move") == std::optional<size_t>(MOVE));
    CHECK_FALSE(FindAction(map, "move").has_value());
    CHECK_FALSE(FindAction(map, "Crouch").has_value());
}

TEST_CASE("Actions - evaluating into a sized state allocates nothing")
{
    const InputActionMap map = TestMap();
    HW::RawInput         input;
    ActionState          state;
    UpdateActionState(map, input, state);

    // Prove the counter counts, then evaluate with it on.
    s_Allocations = 0;
    s_Counting    = true;
    int* probe    = new int(1);
    s_Counting    = false;
    delete probe;
    REQUIRE(s_Allocations > 0);

    s_Allocations = 0;
    s_Counting    = true;
    for (int frame = 0; frame < 100; ++frame)
    {
        SetKey(input, Key::Space, frame % 2 == 0);
        input.CursorDelta = HM::Vector2(static_cast<float>(frame), 0.0f);
        UpdateActionState(map, input, state);
    }
    s_Counting = false;
    CHECK(s_Allocations == 0);
}

TEST_CASE("Actions - the defaults have the game's UI actions and the editor camera's")
{
    const InputActionSet defaults = MakeDefaultInputActions();
    const auto bindingsOf = [](const InputActionMap& map, const char* name) -> const std::vector<InputBinding>&
    {
        const std::optional<size_t> index = FindAction(map, name);
        REQUIRE(index.has_value());
        return map.Actions[*index].Bindings;
    };
    const auto key = [](Key k) { return InputBinding{ BindingSource::Key, static_cast<uint16_t>(k) }; };

    CHECK(defaults.Game.Actions.size() == 6);
    const auto pad   = [](HW::GamepadButton b) { return InputBinding{ BindingSource::GamepadButton, static_cast<uint16_t>(b) }; };
    const auto stick = [](HW::GamepadAxis a, float scale)
    { return InputBinding{ BindingSource::GamepadAxis, static_cast<uint16_t>(a), 0, scale, 0.2f }; };
    CHECK(bindingsOf(defaults.Game, "UiNavigateUp") ==
          std::vector<InputBinding>{ key(Key::Up), pad(HW::GamepadButton::DpadUp), stick(HW::GamepadAxis::LeftY, -1.0f) });
    CHECK(bindingsOf(defaults.Game, "UiNavigateDown") ==
          std::vector<InputBinding>{ key(Key::Down), pad(HW::GamepadButton::DpadDown), stick(HW::GamepadAxis::LeftY, 1.0f) });
    CHECK(bindingsOf(defaults.Game, "UiNavigateLeft") ==
          std::vector<InputBinding>{ key(Key::Left), pad(HW::GamepadButton::DpadLeft), stick(HW::GamepadAxis::LeftX, -1.0f) });
    CHECK(bindingsOf(defaults.Game, "UiNavigateRight") ==
          std::vector<InputBinding>{ key(Key::Right), pad(HW::GamepadButton::DpadRight), stick(HW::GamepadAxis::LeftX, 1.0f) });
    CHECK(bindingsOf(defaults.Game, "UiSubmit") ==
          std::vector<InputBinding>{ key(Key::Enter), key(Key::KeypadEnter), key(Key::Space), pad(HW::GamepadButton::A) });
    CHECK(bindingsOf(defaults.Game, "UiPointerPress") ==
          std::vector<InputBinding>{ { BindingSource::MouseButton, static_cast<uint16_t>(MouseButton::Left) } });

    CHECK(defaults.Editor.Actions.size() == 6);
    const std::vector<InputBinding>& forward = bindingsOf(defaults.Editor, "EditorCameraForward");
    REQUIRE(forward.size() == 1);
    CHECK(forward[0].Source == BindingSource::KeyAxis);
    CHECK(forward[0].Code == static_cast<uint16_t>(Key::W));
    CHECK(forward[0].NegativeCode == static_cast<uint16_t>(Key::S));
    CHECK(bindingsOf(defaults.Editor, "EditorCameraRight")[0].Code == static_cast<uint16_t>(Key::D));
    CHECK(bindingsOf(defaults.Editor, "EditorCameraUp")[0].NegativeCode == static_cast<uint16_t>(Key::Q));
    CHECK(bindingsOf(defaults.Editor, "EditorCameraLookX")[0].Source == BindingSource::PointerDeltaX);
    CHECK(bindingsOf(defaults.Editor, "EditorCameraLookY")[0].Deadzone == 2.0f);
    CHECK(bindingsOf(defaults.Editor, "EditorCameraLookHold").size() == 3);

    // The editor camera's W moves it forward.
    HW::RawInput input;
    SetKey(input, Key::W, true);
    ActionState state;
    UpdateActionState(defaults.Editor, input, state);
    CHECK(GetActionValue(state, *FindAction(defaults.Editor, "EditorCameraForward")) == 1.0f);
}

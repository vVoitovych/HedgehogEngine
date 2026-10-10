#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"

#include "HedgehogCommon/api/Camera.hpp"

#include <cmath>

using HedgehogEngine::EngineContext;

namespace
{
    constexpr float DT           = 0.1f;
    constexpr float CAMERA_SPEED = 10.0f; // Camera's units per second

    HW::RawInput Keys(std::initializer_list<HW::Key> keys)
    {
        HW::RawInput input;
        for (const HW::Key key : keys)
            input.Keys[static_cast<size_t>(key)] = true;
        return input;
    }

    // One headless frame of the editor camera with the given input.
    HM::Vector3 MoveBy(EngineContext& context, const HW::RawInput& input)
    {
        const HM::Vector3 before = context.GetCamera().GetPosition();
        context.UpdateEditorInput(input);
        context.UpdateContext(16.0f / 9.0f, DT);
        return context.GetCamera().GetPosition() - before;
    }

    bool SameMatrix(const HM::Matrix4x4& a, const HM::Matrix4x4& b)
    {
        for (int i = 0; i < 16; ++i)
        {
            if (std::abs(a.GetBuffer()[i] - b.GetBuffer()[i]) > 1e-6f)
                return false;
        }
        return true;
    }
}

TEST_CASE("Editor camera - W, A, D, E and Q move it along the flycam's axes at its speed")
{
    EngineContext context;
    const float   step = CAMERA_SPEED * DT;

    CHECK(MoveBy(context, Keys({ HW::Key::W })) == HM::Vector3(step, 0.0f, 0.0f));
    CHECK(MoveBy(context, Keys({ HW::Key::S })) == HM::Vector3(-step, 0.0f, 0.0f));
    CHECK(MoveBy(context, Keys({ HW::Key::A })) == HM::Vector3(0.0f, step, 0.0f));
    CHECK(MoveBy(context, Keys({ HW::Key::D })) == HM::Vector3(0.0f, -step, 0.0f));
    CHECK(MoveBy(context, Keys({ HW::Key::E })) == HM::Vector3(0.0f, 0.0f, step));
    CHECK(MoveBy(context, Keys({ HW::Key::Q })) == HM::Vector3(0.0f, 0.0f, -step));
    CHECK(MoveBy(context, Keys({})) == HM::Vector3(0.0f, 0.0f, 0.0f));
}

TEST_CASE("Editor camera - the pointer turns it only while the left button is held, and never by a stale move")
{
    EngineContext context;
    context.UpdateEditorInput(HW::RawInput{});
    context.UpdateContext(1.0f, DT);
    const HM::Matrix4x4 start = context.GetCamera().GetViewMatrix();

    HW::RawInput moved;
    moved.CursorDelta = HM::Vector2(40.0f, 0.0f);
    (void)MoveBy(context, moved);
    CHECK(SameMatrix(context.GetCamera().GetViewMatrix(), start)); // no button held

    moved.MouseButtons[static_cast<size_t>(HW::MouseButton::Left)] = true;
    CHECK(MoveBy(context, moved) == HM::Vector3(0.0f, 0.0f, 0.0f)); // turned, not moved
    const HM::Matrix4x4 turned = context.GetCamera().GetViewMatrix();
    CHECK_FALSE(SameMatrix(turned, start));

    // The button still held, the pointer still: the camera stays where it is.
    HW::RawInput still;
    still.MouseButtons[static_cast<size_t>(HW::MouseButton::Left)] = true;
    (void)MoveBy(context, still);
    CHECK(SameMatrix(context.GetCamera().GetViewMatrix(), turned));

    // A move within the 2-pixel deadzone does not turn it either.
    still.CursorDelta = HM::Vector2(2.0f, -1.0f);
    (void)MoveBy(context, still);
    CHECK(SameMatrix(context.GetCamera().GetViewMatrix(), turned));
}

TEST_CASE("Editor camera - right drag pans it along its own right and up, without turning it")
{
    EngineContext context;
    context.UpdateEditorInput(HW::RawInput{});
    context.UpdateContext(1.0f, DT);
    const HM::Matrix4x4 start = context.GetCamera().GetViewMatrix();
    const float         step  = 50.0f * HedgehogEngine::Camera::PAN_UNITS_PER_PIXEL;

    // The default camera looks along +x with +z up, so its right is -y.
    HW::RawInput drag;
    drag.MouseButtons[static_cast<size_t>(HW::MouseButton::Right)] = true;
    drag.CursorDelta = HM::Vector2(50.0f, 0.0f); // the pointer right: the camera left, so the scene follows it
    HM::Vector3 moved = MoveBy(context, drag);
    CHECK(moved.x() == doctest::Approx(0.0f));
    CHECK(moved.y() == doctest::Approx(step));
    CHECK(moved.z() == doctest::Approx(0.0f));

    drag.CursorDelta = HM::Vector2(0.0f, 50.0f); // the pointer down: the camera up
    moved = MoveBy(context, drag);
    CHECK(moved.z() == doctest::Approx(step));
    CHECK(moved.y() == doctest::Approx(0.0f));

    // The middle button pans too; panning never turns the camera.
    HW::RawInput middle;
    middle.MouseButtons[static_cast<size_t>(HW::MouseButton::Middle)] = true;
    middle.CursorDelta = HM::Vector2(0.0f, -50.0f);
    CHECK(MoveBy(context, middle).z() == doctest::Approx(-step));
    const HM::Matrix4x4 view = context.GetCamera().GetViewMatrix();
    for (int column = 0; column < 3; ++column) // the rotation part of the view is unchanged
        for (int row = 0; row < 3; ++row)
            CHECK(view[column][row] == doctest::Approx(start[column][row]));
}

TEST_CASE("Editor camera - the scroll wheel dollies it along its view, by notch")
{
    EngineContext context;
    context.UpdateEditorInput(HW::RawInput{});
    context.UpdateContext(1.0f, DT);

    HW::RawInput wheel;
    wheel.ScrollDelta = HM::Vector2(0.0f, 2.0f); // two notches up: forward
    HM::Vector3 moved = MoveBy(context, wheel);
    CHECK(moved.x() == doctest::Approx(2.0f * HedgehogEngine::Camera::DOLLY_UNITS_PER_STEP));
    CHECK(moved.y() == doctest::Approx(0.0f));
    CHECK(moved.z() == doctest::Approx(0.0f));

    wheel.ScrollDelta = HM::Vector2(0.0f, -1.0f);
    CHECK(MoveBy(context, wheel).x() == doctest::Approx(-HedgehogEngine::Camera::DOLLY_UNITS_PER_STEP));

    // Turned by a left drag, the camera dollies along its new view.
    HW::RawInput look;
    look.MouseButtons[static_cast<size_t>(HW::MouseButton::Left)] = true;
    look.CursorDelta = HM::Vector2(-300.0f, 0.0f); // 45 degrees to the left at 0.15 degrees per pixel
    (void)MoveBy(context, look);
    wheel.ScrollDelta = HM::Vector2(0.0f, 1.0f);
    moved = MoveBy(context, wheel);
    CHECK(moved.x() == doctest::Approx(std::sqrt(0.5f)).epsilon(0.001));
    CHECK(moved.y() == doctest::Approx(std::sqrt(0.5f)).epsilon(0.001));
}

TEST_CASE("Editor camera - it flies in Play too, and the game's input does not move it")
{
    EngineContext context;
    REQUIRE(context.Play());
    CHECK(MoveBy(context, Keys({ HW::Key::W })).x() > 0.0f);

    // Game input alone reaches the game, never the editor camera.
    const HM::Vector3 before = context.GetCamera().GetPosition();
    context.UpdateEditorInput(HW::RawInput{});
    context.UpdateGameInput(Keys({ HW::Key::W }));
    context.UpdateContext(1.0f, DT);
    CHECK(context.GetCamera().GetPosition() == before);
    REQUIRE(context.Stop());
}

#include "doctest/doctest/doctest.h"

#include "Widgets/AxisGizmo.hpp"

#include "HedgehogMath/api/Vector.hpp"

using namespace Editor;

namespace
{
    constexpr size_t X = 0;
    constexpr size_t Y = 1;
    constexpr size_t Z = 2;

    // The editor camera's view: Z up, looking along direction from the origin (Camera::UpdateMatrices).
    [[nodiscard]] HM::Matrix4x4 ViewAlong(const HM::Vector3& direction)
    {
        const HM::Vector3 eye(0.0f, 0.0f, 0.0f);
        return HM::Matrix4x4::LookAt(eye, eye + direction, HM::Vector3(0.0f, 0.0f, 1.0f));
    }
}

TEST_CASE("At yaw 0 the camera looks along world X: Z points up, Y left and X into the screen")
{
    const auto axes = ProjectWorldAxes(ViewAlong(HM::Vector3(1.0f, 0.0f, 0.0f)));

    CHECK(axes[Z].X == doctest::Approx(0.0f));
    CHECK(axes[Z].Y == doctest::Approx(-1.0f)); // screen y grows downwards
    CHECK(axes[Y].X == doctest::Approx(-1.0f));
    CHECK(axes[Y].Y == doctest::Approx(0.0f));
    CHECK(axes[X].X == doctest::Approx(0.0f));
    CHECK(axes[X].Y == doctest::Approx(0.0f));
    CHECK(axes[X].Depth == doctest::Approx(-1.0f)); // pointing away from the viewer
}

TEST_CASE("Turning the camera to look along world Y puts X on the right")
{
    const auto axes = ProjectWorldAxes(ViewAlong(HM::Vector3(0.0f, 1.0f, 0.0f)));

    CHECK(axes[X].X == doctest::Approx(1.0f));
    CHECK(axes[Y].Depth == doctest::Approx(-1.0f));
    CHECK(axes[Z].Y == doctest::Approx(-1.0f));
}

TEST_CASE("Looking down at an angle shortens the vertical axis and keeps every axis unit length in 3D")
{
    const auto axes = ProjectWorldAxes(ViewAlong(HM::Vector3(1.0f, 0.0f, -1.0f)));

    CHECK(axes[Z].Y < 0.0f);
    CHECK(axes[Z].Y > -1.0f);
    for (const ProjectedAxis& axis : axes)
        CHECK(axis.X * axis.X + axis.Y * axis.Y + axis.Depth * axis.Depth == doctest::Approx(1.0f));
}

#include "Widgets/TransformGizmoMath.hpp"

#include "HedgehogMath/api/Quaternion.hpp"

#include "doctest/doctest/doctest.h"

#include <cmath>
#include <string>

using namespace Editor;

namespace
{
    constexpr float EPSILON = 1e-3f;

    bool Near(const HM::Vector3& a, const HM::Vector3& b, float epsilon = EPSILON)
    {
        return std::abs(a.x() - b.x()) < epsilon && std::abs(a.y() - b.y()) < epsilon && std::abs(a.z() - b.z()) < epsilon;
    }

    bool Near(const HM::Matrix4x4& a, const HM::Matrix4x4& b, float epsilon = EPSILON)
    {
        for (size_t column = 0; column < 4; ++column)
        {
            for (size_t row = 0; row < 4; ++row)
            {
                if (std::abs(a[column][row] - b[column][row]) >= epsilon)
                    return false;
            }
        }
        return true;
    }

    LocalTransform Make(HM::Vector3 position, HM::Vector3 rotation, HM::Vector3 scale)
    {
        LocalTransform transform;
        transform.Position = position;
        transform.Rotation = rotation;
        transform.Scale    = scale;
        return transform;
    }

    // The parent of the tests' objects: moved, turned about two axes and uniformly scaled.
    const LocalTransform PARENT = Make(HM::Vector3(3.0f, -2.0f, 5.0f), HM::Vector3(30.0f, -45.0f, 60.0f),
                                       HM::Vector3(2.0f, 2.0f, 2.0f));
}

TEST_CASE("TransformGizmoMath - tool and space names round-trip")
{
    for (const TransformTool tool : { TransformTool::Move, TransformTool::Rotate, TransformTool::Scale })
        CHECK(FindTransformTool(GetTransformToolName(tool)) == tool);
    for (const TransformSpace space : { TransformSpace::Local, TransformSpace::World })
        CHECK(FindTransformSpace(GetTransformSpaceName(space)) == space);
    CHECK(std::string(GetTransformToolName(TransformTool::Rotate)) == "rotate");
    CHECK_FALSE(FindTransformTool("").has_value());
    CHECK_FALSE(FindTransformTool("Move").has_value());
    CHECK_FALSE(FindTransformSpace("global").has_value());
}

TEST_CASE("TransformGizmoMath - ComposeLocalMatrix is T * Rx * Ry * Rz * S")
{
    const LocalTransform transform = Make(HM::Vector3(1.0f, 2.0f, 3.0f), HM::Vector3(10.0f, 20.0f, 30.0f),
                                          HM::Vector3(1.0f, 2.0f, 3.0f));
    const HM::Matrix4x4 expected = HM::Matrix4x4::GetTranslation(1.0f, 2.0f, 3.0f)
                                 * HM::Quaternion::FromEuler(10.0f, 20.0f, 30.0f).ToMatrix()
                                 * HM::Matrix4x4::GetScale(1.0f, 2.0f, 3.0f);
    CHECK(Near(ComposeLocalMatrix(transform), expected));
}

TEST_CASE("TransformGizmoMath - a world matrix under a parent decomposes to the local values that rebuild it")
{
    const HM::Matrix4x4 parentWorld = ComposeLocalMatrix(PARENT);

    SUBCASE("Uniform scale")
    {
        const LocalTransform local = Make(HM::Vector3(1.0f, 0.5f, -2.0f), HM::Vector3(15.0f, 40.0f, -70.0f),
                                          HM::Vector3(1.5f, 1.5f, 1.5f));
        const auto result = DecomposeWorldMatrix(parentWorld * ComposeLocalMatrix(local), parentWorld, local.Rotation);
        REQUIRE(result.has_value());
        CHECK(Near(result->Position, local.Position));
        CHECK(Near(result->Rotation, local.Rotation));
        CHECK(Near(result->Scale, local.Scale));
    }
    SUBCASE("Non-uniform scale")
    {
        const LocalTransform local = Make(HM::Vector3(-4.0f, 0.0f, 1.0f), HM::Vector3(-20.0f, 10.0f, 135.0f),
                                          HM::Vector3(0.5f, 2.0f, 3.0f));
        const auto result = DecomposeWorldMatrix(parentWorld * ComposeLocalMatrix(local), parentWorld, local.Rotation);
        REQUIRE(result.has_value());
        CHECK(Near(result->Position, local.Position));
        CHECK(Near(result->Rotation, local.Rotation));
        CHECK(Near(result->Scale, local.Scale));
    }
    SUBCASE("A mirrored object keeps its rotation and a negative x scale")
    {
        const LocalTransform local = Make(HM::Vector3(0.0f, 0.0f, 0.0f), HM::Vector3(0.0f, 0.0f, 45.0f),
                                          HM::Vector3(-1.0f, 1.0f, 1.0f));
        const auto result = DecomposeWorldMatrix(parentWorld * ComposeLocalMatrix(local), parentWorld, local.Rotation);
        REQUIRE(result.has_value());
        CHECK(Near(result->Rotation, local.Rotation));
        CHECK(Near(result->Scale, local.Scale));
    }
    SUBCASE("At the top level the parent is the identity")
    {
        const LocalTransform local = Make(HM::Vector3(7.0f, 8.0f, 9.0f), HM::Vector3(5.0f, 6.0f, 7.0f),
                                          HM::Vector3(1.0f, 1.0f, 1.0f));
        const auto result = DecomposeWorldMatrix(ComposeLocalMatrix(local), HM::Matrix4x4::GetIdentity(), local.Rotation);
        REQUIRE(result.has_value());
        CHECK(Near(result->Position, local.Position));
        CHECK(Near(result->Rotation, local.Rotation));
    }
}

TEST_CASE("TransformGizmoMath - rotations round-trip through Euler angles, nearest the current ones")
{
    // Every rotation decomposes to angles that rebuild it.
    for (float x = -170.0f; x <= 170.0f; x += 85.0f)
    {
        for (float y = -80.0f; y <= 80.0f; y += 40.0f)
        {
            for (float z = -170.0f; z <= 170.0f; z += 85.0f)
            {
                const LocalTransform local = Make(HM::Vector3(0.0f, 0.0f, 0.0f), HM::Vector3(x, y, z),
                                                  HM::Vector3(1.0f, 1.0f, 1.0f));
                const auto result = DecomposeWorldMatrix(ComposeLocalMatrix(local), HM::Matrix4x4::GetIdentity(),
                                                         HM::Vector3(0.0f, 0.0f, 0.0f));
                REQUIRE(result.has_value());
                CHECK(Near(ComposeLocalMatrix(*result), ComposeLocalMatrix(local)));
            }
        }
    }

    // Angles past what ToEuler gives come back as they were, not as an equal rotation.
    for (const HM::Vector3 rotation : { HM::Vector3(0.0f, 120.0f, 0.0f), HM::Vector3(200.0f, 0.0f, -270.0f),
                                        HM::Vector3(370.0f, -100.0f, 10.0f) })
    {
        const LocalTransform local = Make(HM::Vector3(0.0f, 0.0f, 0.0f), rotation, HM::Vector3(1.0f, 1.0f, 1.0f));
        const auto result = DecomposeWorldMatrix(ComposeLocalMatrix(local), HM::Matrix4x4::GetIdentity(), rotation);
        REQUIRE(result.has_value());
        CHECK(Near(result->Rotation, rotation));
    }
}

TEST_CASE("TransformGizmoMath - a singular matrix has no decomposition")
{
    const HM::Matrix4x4 flat = HM::Matrix4x4::GetScale(1.0f, 0.0f, 1.0f);
    CHECK_FALSE(DecomposeWorldMatrix(flat, HM::Matrix4x4::GetIdentity(), HM::Vector3(0.0f, 0.0f, 0.0f)).has_value());
    CHECK_FALSE(DecomposeWorldMatrix(HM::Matrix4x4::GetIdentity(), flat, HM::Vector3(0.0f, 0.0f, 0.0f)).has_value());
}

TEST_CASE("TransformGizmoMath - a tool changes only its own part")
{
    const LocalTransform dragged = Make(HM::Vector3(1.0f, 1.0f, 1.0f), HM::Vector3(2.0f, 2.0f, 2.0f),
                                        HM::Vector3(3.0f, 3.0f, 3.0f));
    const LocalTransform start = Make(HM::Vector3(0.0f, 0.0f, 0.0f), HM::Vector3(0.0f, 0.0f, 0.0f),
                                      HM::Vector3(1.0f, 1.0f, 1.0f));

    LocalTransform moved = start;
    ApplyTransformTool(TransformTool::Move, dragged, moved);
    CHECK(Near(moved.Position, dragged.Position));
    CHECK(Near(moved.Rotation, start.Rotation));
    CHECK(Near(moved.Scale, start.Scale));

    LocalTransform turned = start;
    ApplyTransformTool(TransformTool::Rotate, dragged, turned);
    CHECK(Near(turned.Position, start.Position));
    CHECK(Near(turned.Rotation, dragged.Rotation));
    CHECK(Near(turned.Scale, start.Scale));

    LocalTransform scaled = start;
    ApplyTransformTool(TransformTool::Scale, dragged, scaled);
    CHECK(Near(scaled.Position, start.Position));
    CHECK(Near(scaled.Rotation, start.Rotation));
    CHECK(Near(scaled.Scale, dragged.Scale));
}

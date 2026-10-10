#pragma once

#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <optional>
#include <string_view>

// The Scene view's transform gizmo without ImGui: its tools and the conversion of a dragged world
// matrix back to the values a TransformComponent stores, so EditorTest can check it.
namespace Editor
{
    // The Scene view's transform tools (keys 1, 2 and 3).
    enum class TransformTool
    {
        Move,
        Rotate,
        Scale
    };

    // The axes the move and rotate handles follow: the object's own or the world's. Scaling always
    // follows the object's own.
    enum class TransformSpace
    {
        Local,
        World
    };

    // The names user://editor_settings.yaml stores: "move", "rotate", "scale"; "local", "world".
    [[nodiscard]] const char*                    GetTransformToolName(TransformTool tool);
    [[nodiscard]] std::optional<TransformTool>   FindTransformTool(std::string_view name);
    [[nodiscard]] const char*                    GetTransformSpaceName(TransformSpace space);
    [[nodiscard]] std::optional<TransformSpace>  FindTransformSpace(std::string_view name);

    // A TransformComponent's values: Rotation is Euler degrees.
    struct LocalTransform
    {
        HM::Vector3 Position = HM::Vector3(0.0f, 0.0f, 0.0f);
        HM::Vector3 Rotation = HM::Vector3(0.0f, 0.0f, 0.0f);
        HM::Vector3 Scale    = HM::Vector3(1.0f, 1.0f, 1.0f);
    };

    // TransformSystem's local matrix: T * Rx * Ry * Rz * S.
    [[nodiscard]] HM::Matrix4x4 ComposeLocalMatrix(const LocalTransform& transform);

    // The local transform whose matrix, under parentWorld, gives world: parentWorld's inverse times
    // world, split into its translation, its axes' lengths as the scale (x negative for a mirroring
    // matrix) and its rotation as Euler degrees, of the two equal solutions the one nearest
    // referenceRotation and each angle within 180 degrees of it, so a drag never makes the
    // inspector's numbers jump to an equal rotation. Exact unless a parent is unevenly scaled and
    // turned (a shear, which no TransformComponent holds). Nullopt for a singular matrix.
    [[nodiscard]] std::optional<LocalTransform> DecomposeWorldMatrix(const HM::Matrix4x4& world,
                                                                     const HM::Matrix4x4& parentWorld,
                                                                     const HM::Vector3&   referenceRotation);

    // What a drag with the tool changes in transform: dragged's position, rotation or scale, the
    // other two kept as they are.
    void ApplyTransformTool(TransformTool tool, const LocalTransform& dragged, LocalTransform& transform);
}

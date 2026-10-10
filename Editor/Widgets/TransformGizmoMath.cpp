#include "TransformGizmoMath.hpp"

#include "HedgehogMath/api/Common.hpp"
#include "HedgehogMath/api/Quaternion.hpp"

#include <array>
#include <cmath>

namespace Editor
{
    namespace
    {
        // An axis shorter than this has no direction to take a rotation from.
        constexpr float MIN_AXIS_LENGTH = 1e-6f;

        HM::Vector3 Column(const HM::Matrix4x4& matrix, size_t column)
        {
            const HM::Vector4& v = matrix[column];
            return HM::Vector3(v.x(), v.y(), v.z());
        }

        // The rotation whose matrix has these unit columns (Shepperd's method).
        HM::Quaternion QuaternionFromAxes(const HM::Vector3& x, const HM::Vector3& y, const HM::Vector3& z)
        {
            // r[row][column]
            const float r[3][3] = { { x.x(), y.x(), z.x() }, { x.y(), y.y(), z.y() }, { x.z(), y.z(), z.z() } };
            const float trace   = r[0][0] + r[1][1] + r[2][2];
            if (trace > 0.0f)
            {
                const float s = std::sqrt(trace + 1.0f) * 2.0f;
                return HM::Quaternion((r[2][1] - r[1][2]) / s, (r[0][2] - r[2][0]) / s, (r[1][0] - r[0][1]) / s,
                                      0.25f * s).Normalize();
            }
            if (r[0][0] > r[1][1] && r[0][0] > r[2][2])
            {
                const float s = std::sqrt(1.0f + r[0][0] - r[1][1] - r[2][2]) * 2.0f;
                return HM::Quaternion(0.25f * s, (r[0][1] + r[1][0]) / s, (r[0][2] + r[2][0]) / s,
                                      (r[2][1] - r[1][2]) / s).Normalize();
            }
            if (r[1][1] > r[2][2])
            {
                const float s = std::sqrt(1.0f + r[1][1] - r[0][0] - r[2][2]) * 2.0f;
                return HM::Quaternion((r[0][1] + r[1][0]) / s, 0.25f * s, (r[1][2] + r[2][1]) / s,
                                      (r[0][2] - r[2][0]) / s).Normalize();
            }
            const float s = std::sqrt(1.0f + r[2][2] - r[0][0] - r[1][1]) * 2.0f;
            return HM::Quaternion((r[0][2] + r[2][0]) / s, (r[1][2] + r[2][1]) / s, 0.25f * s,
                                  (r[1][0] - r[0][1]) / s).Normalize();
        }

        // The angle plus a whole number of turns that lies within 180 degrees of reference.
        float NearestAngle(float angle, float reference)
        {
            return angle + 360.0f * std::round((reference - angle) / 360.0f);
        }

        HM::Vector3 NearestAngles(const HM::Vector3& angles, const HM::Vector3& reference)
        {
            return HM::Vector3(NearestAngle(angles.x(), reference.x()), NearestAngle(angles.y(), reference.y()),
                               NearestAngle(angles.z(), reference.z()));
        }

        float Distance(const HM::Vector3& a, const HM::Vector3& b)
        {
            return std::abs(a.x() - b.x()) + std::abs(a.y() - b.y()) + std::abs(a.z() - b.z());
        }
    }

    const char* GetTransformToolName(TransformTool tool)
    {
        switch (tool)
        {
        case TransformTool::Move:   return "move";
        case TransformTool::Rotate: return "rotate";
        case TransformTool::Scale:  return "scale";
        }
        return "";
    }

    std::optional<TransformTool> FindTransformTool(std::string_view name)
    {
        for (const TransformTool tool : { TransformTool::Move, TransformTool::Rotate, TransformTool::Scale })
        {
            if (name == GetTransformToolName(tool))
                return tool;
        }
        return std::nullopt;
    }

    const char* GetTransformSpaceName(TransformSpace space)
    {
        return space == TransformSpace::Local ? "local" : "world";
    }

    std::optional<TransformSpace> FindTransformSpace(std::string_view name)
    {
        for (const TransformSpace space : { TransformSpace::Local, TransformSpace::World })
        {
            if (name == GetTransformSpaceName(space))
                return space;
        }
        return std::nullopt;
    }

    HM::Matrix4x4 ComposeLocalMatrix(const LocalTransform& transform)
    {
        return HM::Matrix4x4::GetTranslation(transform.Position)
             * HM::Matrix4x4::GetRotationX(HM::ToRadians(transform.Rotation.x()))
             * HM::Matrix4x4::GetRotationY(HM::ToRadians(transform.Rotation.y()))
             * HM::Matrix4x4::GetRotationZ(HM::ToRadians(transform.Rotation.z()))
             * HM::Matrix4x4::GetScale(transform.Scale.x(), transform.Scale.y(), transform.Scale.z());
    }

    std::optional<LocalTransform> DecomposeWorldMatrix(const HM::Matrix4x4& world, const HM::Matrix4x4& parentWorld,
                                                       const HM::Vector3& referenceRotation)
    {
        bool                inverted    = false;
        const HM::Matrix4x4 parentInv   = parentWorld.Inverse(inverted);
        if (!inverted)
            return std::nullopt;
        const HM::Matrix4x4 local = parentInv * world;

        HM::Vector3 x = Column(local, 0);
        HM::Vector3 y = Column(local, 1);
        HM::Vector3 z = Column(local, 2);
        float scaleX = x.LengthSlow();
        const float scaleY = y.LengthSlow();
        const float scaleZ = z.LengthSlow();
        if (scaleX < MIN_AXIS_LENGTH || scaleY < MIN_AXIS_LENGTH || scaleZ < MIN_AXIS_LENGTH)
            return std::nullopt;
        x = x / scaleX;
        y = y / scaleY;
        z = z / scaleZ;
        // A mirroring matrix: the x axis carries the reflection, so the rest is a rotation.
        if (HM::Dot(HM::Cross(x, y), z) < 0.0f)
        {
            scaleX = -scaleX;
            x      = -x;
        }

        const HM::Vector3 euler = QuaternionFromAxes(x, y, z).ToEuler();
        // Rx(a) Ry(b) Rz(c) equals Rx(a + 180) Ry(180 - b) Rz(c + 180): take the one nearer the reference.
        const std::array<HM::Vector3, 2> candidates = {
            NearestAngles(euler, referenceRotation),
            NearestAngles(HM::Vector3(euler.x() + 180.0f, 180.0f - euler.y(), euler.z() + 180.0f), referenceRotation)
        };
        const bool second = Distance(candidates[1], referenceRotation) < Distance(candidates[0], referenceRotation);

        LocalTransform result;
        result.Position = Column(local, 3);
        result.Rotation = candidates[second ? 1 : 0];
        result.Scale    = HM::Vector3(scaleX, scaleY, scaleZ);
        return result;
    }

    void ApplyTransformTool(TransformTool tool, const LocalTransform& dragged, LocalTransform& transform)
    {
        switch (tool)
        {
        case TransformTool::Move:   transform.Position = dragged.Position; break;
        case TransformTool::Rotate: transform.Rotation = dragged.Rotation; break;
        case TransformTool::Scale:  transform.Scale    = dragged.Scale;    break;
        }
    }
}

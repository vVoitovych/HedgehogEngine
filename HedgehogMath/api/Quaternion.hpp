#pragma once
#include "Vector.hpp"
#include "HedgehogMathAPI.hpp"

namespace HM
{
    class Matrix4x4;

    // A rotation as a unit quaternion x*i + y*j + z*k + w. Rotations compose like the matrices they
    // stand for: (a * b) rotates by b first, then by a, and a.ToMatrix() * b.ToMatrix() equals
    // (a * b).ToMatrix().
    //
    // Euler angles are in degrees, in TransformSystem's order: FromEuler(e).ToMatrix() equals
    // GetRotationX(e.x) * GetRotationY(e.y) * GetRotationZ(e.z), so a point turns about Z first,
    // then Y, then X. TransformComponent keeps its rotation as these Euler angles.
    class Quaternion
    {
    public:
        // The identity rotation.
        HEDGEHOG_MATH_API Quaternion();
        HEDGEHOG_MATH_API Quaternion(float x, float y, float z, float w);

        HEDGEHOG_MATH_API float x() const;
        HEDGEHOG_MATH_API float y() const;
        HEDGEHOG_MATH_API float z() const;
        HEDGEHOG_MATH_API float w() const;

        [[nodiscard]] HEDGEHOG_MATH_API static Quaternion Identity();
        // A turn of angleDegrees about axis, counterclockwise looking down the axis at the origin.
        // The axis need not be unit length; a zero axis gives the identity.
        [[nodiscard]] HEDGEHOG_MATH_API static Quaternion FromAxisAngle(const Vector3& axis, float angleDegrees);
        [[nodiscard]] HEDGEHOG_MATH_API static Quaternion FromEuler(const Vector3& eulerDegrees);
        [[nodiscard]] HEDGEHOG_MATH_API static Quaternion FromEuler(float xDegrees, float yDegrees, float zDegrees);
        // The rotation that turns the local -Z axis (a camera's view direction) to forward and
        // the local +Y axis as close to up as it can go. When forward is parallel to up another
        // up is chosen; a zero forward gives the identity.
        [[nodiscard]] HEDGEHOG_MATH_API static Quaternion LookRotation(const Vector3& forward,
                                                                       const Vector3& up = Vector3(0.0f, 1.0f, 0.0f));
        // Constant angular speed from a (t = 0) to b (t = 1), along the shorter way round.
        [[nodiscard]] HEDGEHOG_MATH_API static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);

        // Euler angles in degrees with x and z in [-180, 180] and y in [-90, 90]. At y = +-90
        // (gimbal lock) z is 0 and x carries the whole turn.
        [[nodiscard]] HEDGEHOG_MATH_API Vector3 ToEuler() const;
        [[nodiscard]] HEDGEHOG_MATH_API Matrix4x4 ToMatrix() const;

        [[nodiscard]] HEDGEHOG_MATH_API Quaternion operator*(const Quaternion& other) const;
        [[nodiscard]] HEDGEHOG_MATH_API Vector3 operator*(const Vector3& vector) const;
        [[nodiscard]] HEDGEHOG_MATH_API Vector3 Rotate(const Vector3& vector) const;

        [[nodiscard]] HEDGEHOG_MATH_API float Length() const;
        // A zero quaternion normalizes to the identity.
        [[nodiscard]] HEDGEHOG_MATH_API Quaternion Normalize() const;
        [[nodiscard]] HEDGEHOG_MATH_API Quaternion Conjugate() const;
        // The opposite rotation; for a unit quaternion this is the conjugate.
        [[nodiscard]] HEDGEHOG_MATH_API Quaternion Inverse() const;

        HEDGEHOG_MATH_API bool operator==(const Quaternion& other) const;
        HEDGEHOG_MATH_API bool operator!=(const Quaternion& other) const;

    private:
        float m_X;
        float m_Y;
        float m_Z;
        float m_W;
    };

    HEDGEHOG_MATH_API float Dot(const Quaternion& first, const Quaternion& second);
}

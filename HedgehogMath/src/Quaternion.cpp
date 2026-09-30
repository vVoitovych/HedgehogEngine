#include "api/Quaternion.hpp"
#include "api/Matrix.hpp"
#include "api/Common.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    // Below this, a sum of squares is treated as zero.
    constexpr float EPSILON_SQR = 1e-12f;
    // Above this cosine the two rotations are so close that Slerp falls back to a normalized lerp.
    constexpr float SLERP_LINEAR_THRESHOLD = 0.9995f;
    // Below this, cos(y) of an Euler decomposition is treated as zero (gimbal lock).
    constexpr double GIMBAL_EPSILON = 1e-6;

    // The quaternion of a pure rotation matrix given as m[row][column] (Shepperd's method: divide
    // by the largest of the four diagonal combinations, so no division is close to zero).
    HM::Quaternion FromRotationMatrix(const double m[3][3])
    {
        const double trace = m[0][0] + m[1][1] + m[2][2];
        double x, y, z, w;
        if (trace > 0.0)
        {
            const double s = std::sqrt(trace + 1.0) * 2.0;
            w = 0.25 * s;
            x = (m[2][1] - m[1][2]) / s;
            y = (m[0][2] - m[2][0]) / s;
            z = (m[1][0] - m[0][1]) / s;
        }
        else if (m[0][0] > m[1][1] && m[0][0] > m[2][2])
        {
            const double s = std::sqrt(1.0 + m[0][0] - m[1][1] - m[2][2]) * 2.0;
            w = (m[2][1] - m[1][2]) / s;
            x = 0.25 * s;
            y = (m[0][1] + m[1][0]) / s;
            z = (m[0][2] + m[2][0]) / s;
        }
        else if (m[1][1] > m[2][2])
        {
            const double s = std::sqrt(1.0 + m[1][1] - m[0][0] - m[2][2]) * 2.0;
            w = (m[0][2] - m[2][0]) / s;
            x = (m[0][1] + m[1][0]) / s;
            y = 0.25 * s;
            z = (m[1][2] + m[2][1]) / s;
        }
        else
        {
            const double s = std::sqrt(1.0 + m[2][2] - m[0][0] - m[1][1]) * 2.0;
            w = (m[1][0] - m[0][1]) / s;
            x = (m[0][2] + m[2][0]) / s;
            y = (m[1][2] + m[2][1]) / s;
            z = 0.25 * s;
        }
        return HM::Quaternion(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z),
                              static_cast<float>(w)).Normalize();
    }
}

namespace HM
{
    Quaternion::Quaternion()
        : m_X(0.0f), m_Y(0.0f), m_Z(0.0f), m_W(1.0f)
    {
    }

    Quaternion::Quaternion(float x, float y, float z, float w)
        : m_X(x), m_Y(y), m_Z(z), m_W(w)
    {
    }

    float Quaternion::x() const { return m_X; }
    float Quaternion::y() const { return m_Y; }
    float Quaternion::z() const { return m_Z; }
    float Quaternion::w() const { return m_W; }

    Quaternion Quaternion::Identity()
    {
        return Quaternion();
    }

    Quaternion Quaternion::FromAxisAngle(const Vector3& axis, float angleDegrees)
    {
        const float lengthSqr = axis.LengthSqr();
        if (lengthSqr < EPSILON_SQR)
            return Quaternion();

        const double half  = ToRadians(static_cast<double>(angleDegrees)) * 0.5;
        const float  scale = static_cast<float>(std::sin(half)) / std::sqrt(lengthSqr);
        return Quaternion(axis.x() * scale, axis.y() * scale, axis.z() * scale, static_cast<float>(std::cos(half)));
    }

    Quaternion Quaternion::FromEuler(const Vector3& eulerDegrees)
    {
        return FromEuler(eulerDegrees.x(), eulerDegrees.y(), eulerDegrees.z());
    }

    Quaternion Quaternion::FromEuler(float xDegrees, float yDegrees, float zDegrees)
    {
        // qx * qy * qz, multiplied out.
        const double hx = ToRadians(static_cast<double>(xDegrees)) * 0.5;
        const double hy = ToRadians(static_cast<double>(yDegrees)) * 0.5;
        const double hz = ToRadians(static_cast<double>(zDegrees)) * 0.5;
        const double cx = std::cos(hx), sx = std::sin(hx);
        const double cy = std::cos(hy), sy = std::sin(hy);
        const double cz = std::cos(hz), sz = std::sin(hz);

        return Quaternion(static_cast<float>(sx * cy * cz + cx * sy * sz),
                          static_cast<float>(cx * sy * cz - sx * cy * sz),
                          static_cast<float>(cx * cy * sz + sx * sy * cz),
                          static_cast<float>(cx * cy * cz - sx * sy * sz));
    }

    Quaternion Quaternion::LookRotation(const Vector3& forward, const Vector3& up)
    {
        if (forward.LengthSqr() < EPSILON_SQR)
            return Quaternion();

        const Vector3 back  = (-forward).Normalize();
        Vector3       right = Cross(up, back);
        if (right.LengthSqr() < EPSILON_SQR)
        {
            // up is parallel to forward: any perpendicular up will do.
            const Vector3 fallback = std::abs(back.y()) < 0.9f ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(0.0f, 0.0f, 1.0f);
            right = Cross(fallback, back);
        }
        right                    = right.Normalize();
        const Vector3 trueUp     = Cross(back, right);

        // The columns of the rotation are the local axes: +X right, +Y up, +Z back.
        const double m[3][3] = {
            { right.x(), trueUp.x(), back.x() },
            { right.y(), trueUp.y(), back.y() },
            { right.z(), trueUp.z(), back.z() },
        };
        return FromRotationMatrix(m);
    }

    Quaternion Quaternion::Slerp(const Quaternion& a, const Quaternion& b, float t)
    {
        Quaternion end    = b;
        float      cosine = Dot(a, b);
        if (cosine < 0.0f)
        {
            // q and -q are the same rotation; the negated one is the shorter way round.
            end    = Quaternion(-b.m_X, -b.m_Y, -b.m_Z, -b.m_W);
            cosine = -cosine;
        }

        float weightA = 1.0f - t;
        float weightB = t;
        if (cosine < SLERP_LINEAR_THRESHOLD)
        {
            const float angle = std::acos(cosine);
            const float sine  = std::sin(angle);
            weightA           = std::sin(weightA * angle) / sine;
            weightB           = std::sin(weightB * angle) / sine;
        }

        return Quaternion(weightA * a.m_X + weightB * end.m_X, weightA * a.m_Y + weightB * end.m_Y,
                          weightA * a.m_Z + weightB * end.m_Z, weightA * a.m_W + weightB * end.m_W)
            .Normalize();
    }

    Vector3 Quaternion::ToEuler() const
    {
        const Quaternion q = Normalize();
        const double     x = q.m_X, y = q.m_Y, z = q.m_Z, w = q.m_W;

        // The entries of Rx(a) * Ry(b) * Rz(c) this needs:
        //   m00 = cb cc   m01 = -cb sc   m02 = sb
        //   m11 = ca cc - sa sb sc       m12 = -sa cb
        //   m21 = sa cc + ca sb sc       m22 = ca cb
        const double m00 = 1.0 - 2.0 * (y * y + z * z);
        const double m01 = 2.0 * (x * y - z * w);
        const double m02 = 2.0 * (x * z + y * w);
        const double m11 = 1.0 - 2.0 * (x * x + z * z);
        const double m12 = 2.0 * (y * z - x * w);
        const double m21 = 2.0 * (y * z + x * w);
        const double m22 = 1.0 - 2.0 * (x * x + y * y);

        // atan2 rather than asin: well conditioned near +-90 degrees too.
        const double cosY = std::sqrt(m00 * m00 + m01 * m01);
        const double angleY = std::atan2(m02, cosY);
        double angleX, angleZ;
        if (cosY > GIMBAL_EPSILON)
        {
            angleX = std::atan2(-m12, m22);
            angleZ = std::atan2(-m01, m00);
        }
        else
        {
            // Gimbal lock: only x + z (or x - z) is defined. With c = 0, m21 = sa and m11 = ca.
            angleX = std::atan2(m21, m11);
            angleZ = 0.0;
        }

        return Vector3(static_cast<float>(ToDegree(angleX)), static_cast<float>(ToDegree(angleY)),
                       static_cast<float>(ToDegree(angleZ)));
    }

    Matrix4x4 Quaternion::ToMatrix() const
    {
        const float xx = m_X * m_X, yy = m_Y * m_Y, zz = m_Z * m_Z;
        const float xy = m_X * m_Y, xz = m_X * m_Z, yz = m_Y * m_Z;
        const float wx = m_W * m_X, wy = m_W * m_Y, wz = m_W * m_Z;

        // Matrix4x4 stores columns: each Vector4 below is one column of the rotation.
        return Matrix4x4{
            { 1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz), 2.0f * (xz - wy), 0.0f },
            { 2.0f * (xy - wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx), 0.0f },
            { 2.0f * (xz + wy), 2.0f * (yz - wx), 1.0f - 2.0f * (xx + yy), 0.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f },
        };
    }

    Quaternion Quaternion::operator*(const Quaternion& other) const
    {
        return Quaternion(m_W * other.m_X + m_X * other.m_W + m_Y * other.m_Z - m_Z * other.m_Y,
                          m_W * other.m_Y - m_X * other.m_Z + m_Y * other.m_W + m_Z * other.m_X,
                          m_W * other.m_Z + m_X * other.m_Y - m_Y * other.m_X + m_Z * other.m_W,
                          m_W * other.m_W - m_X * other.m_X - m_Y * other.m_Y - m_Z * other.m_Z);
    }

    Vector3 Quaternion::operator*(const Vector3& vector) const
    {
        return Rotate(vector);
    }

    Vector3 Quaternion::Rotate(const Vector3& vector) const
    {
        // q v q*, expanded: v + w t + u x t with u the vector part and t = 2 u x v.
        const Vector3 u(m_X, m_Y, m_Z);
        const Vector3 t = Cross(u, vector) * 2.0f;
        return vector + t * m_W + Cross(u, t);
    }

    float Quaternion::Length() const
    {
        return std::sqrt(Dot(*this, *this));
    }

    Quaternion Quaternion::Normalize() const
    {
        const float lengthSqr = Dot(*this, *this);
        if (lengthSqr < EPSILON_SQR)
            return Quaternion();

        const float inverseLength = 1.0f / std::sqrt(lengthSqr);
        return Quaternion(m_X * inverseLength, m_Y * inverseLength, m_Z * inverseLength, m_W * inverseLength);
    }

    Quaternion Quaternion::Conjugate() const
    {
        return Quaternion(-m_X, -m_Y, -m_Z, m_W);
    }

    Quaternion Quaternion::Inverse() const
    {
        const float lengthSqr = Dot(*this, *this);
        if (lengthSqr < EPSILON_SQR)
            return Quaternion();

        return Quaternion(-m_X / lengthSqr, -m_Y / lengthSqr, -m_Z / lengthSqr, m_W / lengthSqr);
    }

    bool Quaternion::operator==(const Quaternion& other) const
    {
        return m_X == other.m_X && m_Y == other.m_Y && m_Z == other.m_Z && m_W == other.m_W;
    }

    bool Quaternion::operator!=(const Quaternion& other) const
    {
        return !(*this == other);
    }

    float Dot(const Quaternion& first, const Quaternion& second)
    {
        return first.x() * second.x() + first.y() * second.y() + first.z() * second.z() + first.w() * second.w();
    }
}

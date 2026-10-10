#include "api/Camera.hpp"

#include <cmath>

namespace HedgehogEngine
{
    void Camera::UpdateCamera(
        float dt,
        float ratio,
        const HM::Vector3& posOffset,
        const HM::Vector2& dirOffset)
    {
        m_Aspect = ratio;

        m_Pos += posOffset * m_CameraSpeed * dt;

        float xoffset = dirOffset.x();
        float yoffset = dirOffset.y();

        xoffset *= m_MouseSensitivity;
        yoffset *= m_MouseSensitivity;

        m_Yaw   -= xoffset;
        m_Pitch -= yoffset;

        if (m_Pitch > 80.0f)
            m_Pitch = 80.0f;
        if (m_Pitch < -80.0f)
            m_Pitch = -80.0f;

        m_Direction.x() = std::cos(HM::ToRadians(m_Yaw))   * std::cos(HM::ToRadians(m_Pitch));
        m_Direction.y() = std::sin(HM::ToRadians(m_Yaw))   * std::cos(HM::ToRadians(m_Pitch));
        m_Direction.z() = std::sin(HM::ToRadians(m_Pitch));

        m_RightVector = (Cross(m_Direction, m_UpVector)).Normalize();

        UpdateMatrices();
    }

    void Camera::Pan(float dx, float dy)
    {
        if (dx == 0.0f && dy == 0.0f)
            return;
        // The camera's own up: perpendicular to its view and right, so a pitched camera pans in its
        // image plane.
        const HM::Vector3 up    = Cross(m_RightVector, m_Direction).Normalize();
        const float       scale = PAN_UNITS_PER_PIXEL * m_CameraSpeed / DEFAULT_SPEED;
        m_Pos -= m_RightVector * (dx * scale);
        m_Pos += up * (dy * scale);
        UpdateMatrices();
    }

    void Camera::Dolly(float steps)
    {
        if (steps == 0.0f)
            return;
        m_Pos += m_Direction * (steps * DOLLY_UNITS_PER_STEP * m_CameraSpeed / DEFAULT_SPEED);
        UpdateMatrices();
    }

    void Camera::SetFov(float fov)
    {
        m_FOV = fov;
    }

    void Camera::SetAspect(float aspect)
    {
        m_Aspect = aspect;
    }

    void Camera::SetNearPlane(float nearPlane)
    {
        m_NearPlane = nearPlane;
    }

    void Camera::SetFarPlane(float farPlane)
    {
        m_FarPlane = farPlane;
    }

    HM::Matrix4x4 Camera::GetViewMatrix() const
    {
        return m_ViewMatrix;
    }

    HM::Matrix4x4 Camera::GetProjectionMatrix() const
    {
        return m_ProjMatrix;
    }

    HM::Vector3 Camera::GetPosition() const
    {
        return m_Pos;
    }

    float Camera::GetFov() const
    {
        return m_FOV;
    }

    float Camera::GetNearPlane() const
    {
        return m_NearPlane;
    }

    float Camera::GetFarPlane() const
    {
        return m_FarPlane;
    }

    void Camera::UpdateMatrices()
    {
        m_ViewMatrix = HM::Matrix4x4::LookAt(m_Pos, m_Pos + m_Direction, m_UpVector);
        m_ProjMatrix = HM::Matrix4x4::Perspective(m_FOV / m_Aspect, m_Aspect, m_NearPlane, m_FarPlane);
        m_ProjMatrix[1][1] *= -1;
    }
}

#include "Camera.hpp"

namespace Context
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

        m_Yaw -= xoffset;
        m_Pitch -= yoffset;

        if (m_Pitch > 80.0f)
            m_Pitch = 80.0f;
        if (m_Pitch < -80.0f)
            m_Pitch = -80.0f;

        m_Direction.x() = cosf(HM::ToRadians(m_Yaw)) * cosf(HM::ToRadians(m_Pitch));
        m_Direction.y() = sinf(HM::ToRadians(m_Yaw)) * cosf(HM::ToRadians(m_Pitch));
        m_Direction.z() = sinf(HM::ToRadians(m_Pitch));

        m_RightVector = (Cross(m_Direction, m_UpVector)).Normalize();

        UpdateMatricies();
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

    HM::Matrix4x4 Camera::GetViewProjectionMatrix() const
    {
        return m_ViewProjMatrix;;
    }

    HM::Vector3 Camera::GetPosition() const
    {
        return m_Pos;
    }

    float Camera::GetNearPlane() const
    {
        return m_NearPlane;
    }

    float Camera::GetFarPlane() const
    {
        return m_FarPlane;
    }

    void Camera::UpdateMatricies()
    {
        m_ViewMatrix = HM::Matrix4x4::LookAt(m_Pos, m_Pos + m_Direction, m_UpVector);
        m_ProjMatrix = HM::Matrix4x4::Perspective(m_FOV / m_Aspect, m_Aspect, m_NearPlane, m_FarPlane);
        m_ProjMatrix[1][1] *= -1;
        m_ViewProjMatrix = m_ProjMatrix * m_ViewMatrix;
    }

}


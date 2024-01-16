#include "Camera.hpp"

#include "Renderer/WindowManagment/Controls.hpp"
#include "Logger/Logger.hpp"

namespace Renderer
{
    void Camera::UpdateCamera(float dt, float ratio, Controls& controls)
    {
        mAspect = ratio;

        if (controls.IsPressedQ)
        {
            mPos -= mUpVector * mCameraSpeed * dt;
        }
        if (controls.IsPressedE)
        {
            mPos += mUpVector * mCameraSpeed * dt;
        }

        if (controls.IsPressedW)
        {
            mPos += mDirection * mCameraSpeed * dt;
        }
        if (controls.IsPressedS)
        {
            mPos -= mDirection * mCameraSpeed * dt;
        }

        if (controls.IsPressedD)
        {
            mPos += mRightVector * mCameraSpeed * dt;
        }
        if (controls.IsPressedA)
        {
            mPos -= mRightVector * mCameraSpeed * dt;
        }

        float xoffset = controls.MouseDelta.x;
        float yoffset = controls.MouseDelta.y;

        xoffset *= mMouseSensitivity;
        yoffset *= mMouseSensitivity;

        mYaw -= xoffset;
        mPitch -= yoffset;

        if (mPitch > 80.0f)
            mPitch = 80.0f;
        if (mPitch < -80.0f)
            mPitch = -80.0f;

        glm::vec3 direction;
        direction.x = cos(glm::radians(mPitch)) * cos(glm::radians(mYaw));
        direction.y = sin(glm::radians(mYaw));
        direction.z = sin(glm::radians(mPitch)) * cos(glm::radians(mYaw));
        mDirection = glm::normalize(direction);

        mRightVector = glm::normalize(glm::cross(mDirection, mUpVector));

        UpdateMatricies();
    }

    void Camera::SetFov(float fov)
    {
        mFOV = fov;
    }

    void Camera::SetAspect(float aspect)
    {
        mAspect = aspect;
    }

    void Camera::SetNearPlane(float nearPlane)
    {
        mNearPlane = nearPlane;
    }

    void Camera::SetFarPlane(float farPlane)
    {
        mFarPlane = farPlane;
    }

    glm::mat4 Camera::GetViewMatrix() const 
    {
        return mViewMatrix;
    }

    glm::mat4 Camera::GetProjectionMatrix() const 
    {
        return mProjMatrix;
    }

    glm::vec3 Camera::GetPosition() const
    {
        return mPos;
    }

    void Camera::UpdateMatricies()
    {
        mViewMatrix = glm::lookAt(mPos, mPos + mDirection, mUpVector);
        mProjMatrix = glm::perspective(mFOV, mAspect, mNearPlane, mFarPlane);
        mProjMatrix[1][1] *= -1;
    }

}


#include "Camera.h"

#include "VulkanEngine/Renderer/WindowManagment/Controls.h"
#include "VulkanEngine/Logger/Logger.h"

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

        mYaw += yoffset;
        mPitch += xoffset;

        if (mPitch > 80.0f)
            mPitch = 80.0f;
        if (mPitch < -80.0f)
            mPitch = -80.0f;

        glm::vec3 direction;
        direction.x = cos(glm::radians(mYaw)) * cos(glm::radians(mPitch));
        direction.y = sin(glm::radians(mPitch));
        direction.z = sin(glm::radians(mYaw)) * cos(glm::radians(mPitch));
        mDirection = glm::normalize(direction);
        glm::normalize(glm::cross(mDirection, mUpVector));

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

    glm::mat4 Camera::GetViewMatrix()
    {
        return mViewMatrix;
    }

    glm::mat4 Camera::GetProjectionMatrix()
    {
        return mProjMatrix;
    }

    void Camera::UpdateMatricies()
    {
        mViewMatrix = glm::lookAt(mPos, mPos + mDirection, mUpVector);
        mProjMatrix = glm::perspective(mFOV, mAspect, mNearPlane, mFarPlane);
        mProjMatrix[1][1] *= -1;
    }

}


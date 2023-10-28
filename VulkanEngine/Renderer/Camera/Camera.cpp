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
            mPos += mUpVector * mCameraSpeed * dt;
        }
        if (controls.IsPressedE)
        {
            mPos -= mUpVector * mCameraSpeed * dt;
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


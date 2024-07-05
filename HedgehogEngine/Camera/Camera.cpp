#include "Camera.hpp"
#include "WindowManagment/Controls.hpp"

#include "HedgehogMath/Common.hpp"

#include "Logger/Logger.hpp"

namespace Context
{
    void Camera::UpdateCamera(float dt, float ratio, const WinManager::Controls& controls)
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

        float xoffset = controls.MouseDelta.x();
        float yoffset = controls.MouseDelta.y();

        xoffset *= mMouseSensitivity;
        yoffset *= mMouseSensitivity;

        mYaw -= xoffset;
        mPitch -= yoffset;

        if (mPitch > 80.0f)
            mPitch = 80.0f;
        if (mPitch < -80.0f)
            mPitch = -80.0f;

        mDirection.x() = cos(HM::ToRadians(mYaw)) * cos(HM::ToRadians(mPitch));
        mDirection.y() = sin(HM::ToRadians(mYaw)) * cos(HM::ToRadians(mPitch));
        mDirection.z() = sin(HM::ToRadians(mPitch));

        mRightVector = (mDirection.Cross(mUpVector)).Normalize();

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

    HM::Matrix4x4 Camera::GetViewMatrix() const
    {
        return mViewMatrix;
    }

    HM::Matrix4x4 Camera::GetProjectionMatrix() const
    {
        return mProjMatrix;
    }

    HM::Vector3 Camera::GetPosition() const
    {
        return mPos;
    }

    void Camera::UpdateMatricies()
    {
        mViewMatrix = HM::Matrix4x4::LookAt(mPos, mPos + mDirection, mUpVector);
        mProjMatrix = HM::Matrix4x4::CalculateProjPerspective(mFOV, mAspect, mNearPlane, mFarPlane);
        mProjMatrix[1][1] *= -1;
    }

}


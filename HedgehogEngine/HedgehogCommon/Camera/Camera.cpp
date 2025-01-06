#include "Camera.hpp"

#include "HedgehogMath/Common.hpp"

namespace Context
{
    void Camera::UpdateCamera(
        float dt, 
        float ratio, 
        const HM::Vector3& posOffset,
        const HM::Vector2& dirOffset)
    {
        mAspect = ratio;

        mPos += posOffset * mCameraSpeed * dt;
        

        float xoffset = dirOffset.x();
        float yoffset = dirOffset.y();

        xoffset *= mMouseSensitivity;
        yoffset *= mMouseSensitivity;

        mYaw -= xoffset;
        mPitch -= yoffset;

        if (mPitch > 80.0f)
            mPitch = 80.0f;
        if (mPitch < -80.0f)
            mPitch = -80.0f;

        mDirection.x() = cosf(HM::ToRadians(mYaw)) * cosf(HM::ToRadians(mPitch));
        mDirection.y() = sinf(HM::ToRadians(mYaw)) * cosf(HM::ToRadians(mPitch));
        mDirection.z() = sinf(HM::ToRadians(mPitch));

        mRightVector = (Cross(mDirection, mUpVector)).Normalize();

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
        mProjMatrix = HM::Matrix4x4::Perspective(mFOV / mAspect, mAspect, mNearPlane, mFarPlane);
        mProjMatrix[1][1] *= -1;
    }

}


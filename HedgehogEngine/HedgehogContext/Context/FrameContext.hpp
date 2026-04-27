#pragma once

#include "HedgehogContext/api/HedgehogContextApi.hpp"

#include "HedgehogMath/api/Vector.hpp"
#include "HedgehogMath/api/Matrix.hpp"

namespace Context
{
    class Camera;

    class FrameContext
    {
    public:
        FrameContext() = default;
        ~FrameContext() = default;

        FrameContext(const FrameContext&) = delete;
        FrameContext& operator=(const FrameContext&) = delete;

        HEDGEHOG_CONTEXT_API void UpdateContext(const Camera& camera);

        HEDGEHOG_CONTEXT_API HM::Matrix4x4 GetCameraViewMatrix() const;
        HEDGEHOG_CONTEXT_API HM::Matrix4x4 GetCameraProjMatrix() const;
        HEDGEHOG_CONTEXT_API HM::Matrix4x4 GetCameraViewProjMatrix() const;

        HEDGEHOG_CONTEXT_API HM::Matrix4x4 GetCameraInvViewMatrix() const;
        HEDGEHOG_CONTEXT_API HM::Matrix4x4 GetCameraInvProjMatrix() const;
        HEDGEHOG_CONTEXT_API HM::Matrix4x4 GetCameraInvViewProjMatrix() const;

        HEDGEHOG_CONTEXT_API HM::Vector3 GetCameraPosition() const;

        HEDGEHOG_CONTEXT_API float GetDeltaTime() const;

        HEDGEHOG_CONTEXT_API void     SetBackBufferIndex(uint32_t index);
        HEDGEHOG_CONTEXT_API uint32_t GetBackBufferIndex() const;

    private:
        HM::Matrix4x4 m_CameraViewMatrix;
        HM::Matrix4x4 m_CameraProjMatrix;
        HM::Matrix4x4 m_CameraViewProjMatrix;

        HM::Matrix4x4 m_CameraInvViewMatrix;
        HM::Matrix4x4 m_CameraInvProjMatrix;
        HM::Matrix4x4 m_CameraInvViewProjMatrix;

        HM::Vector3 m_CameraPosition;

        float    m_DeltaTime;
        uint32_t m_BackBufferIndex;

    };

}


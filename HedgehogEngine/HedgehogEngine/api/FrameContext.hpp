#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "HedgehogMath/api/Vector.hpp"
#include "HedgehogMath/api/Matrix.hpp"

namespace HedgehogEngine
{
    class Camera;

    class FrameContext
    {
    public:
        FrameContext()  = default;
        ~FrameContext() = default;

        FrameContext(const FrameContext&)            = delete;
        FrameContext& operator=(const FrameContext&) = delete;

        HEDGEHOG_ENGINE_API void UpdateContext(const Camera& camera);

        HEDGEHOG_ENGINE_API HM::Matrix4x4 GetCameraViewMatrix()        const;
        HEDGEHOG_ENGINE_API HM::Matrix4x4 GetCameraProjMatrix()        const;
        HEDGEHOG_ENGINE_API HM::Matrix4x4 GetCameraViewProjMatrix()    const;

        HEDGEHOG_ENGINE_API HM::Matrix4x4 GetCameraInvViewMatrix()     const;
        HEDGEHOG_ENGINE_API HM::Matrix4x4 GetCameraInvProjMatrix()     const;
        HEDGEHOG_ENGINE_API HM::Matrix4x4 GetCameraInvViewProjMatrix() const;

        HEDGEHOG_ENGINE_API HM::Vector3 GetCameraPosition() const;

        HEDGEHOG_ENGINE_API float GetDeltaTime() const;

        HEDGEHOG_ENGINE_API void     SetBackBufferIndex(uint32_t index);
        HEDGEHOG_ENGINE_API uint32_t GetBackBufferIndex()               const;

    private:
        HM::Matrix4x4 m_CameraViewMatrix;
        HM::Matrix4x4 m_CameraProjMatrix;
        HM::Matrix4x4 m_CameraViewProjMatrix;

        HM::Matrix4x4 m_CameraInvViewMatrix;
        HM::Matrix4x4 m_CameraInvProjMatrix;
        HM::Matrix4x4 m_CameraInvViewProjMatrix;

        HM::Vector3 m_CameraPosition;

        float    m_DeltaTime       = 0.0f;
        uint32_t m_BackBufferIndex = 0;
    };
}

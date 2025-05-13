#pragma once

#include "HedgehogMath/Vector.hpp"
#include "HedgehogMath/Matrix.hpp"

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

		void UpdateContext(const Camera& camera);

		HM::Matrix4x4 GetCameraViewMatrix() const;
		HM::Matrix4x4 GetCameraProjMatrix() const;
		HM::Matrix4x4 GetCameraViewProjMatrix() const;

		HM::Matrix4x4 GetCameraInvViewMatrix() const;
		HM::Matrix4x4 GetCameraInvProjMatrix() const;
		HM::Matrix4x4 GetCameraInvViewProjMatrix() const;

		HM::Vector3 GetCameraPosition() const;

		float GetDeltaTime() const;

		void SetBackBufferIndex(uint32_t index);
		uint32_t GetBackBufferIndex() const;

	private:
		HM::Matrix4x4 m_CameraViewMatrix;
		HM::Matrix4x4 m_CameraProjMatrix;
		HM::Matrix4x4 m_CameraViewProjMatrix;

		HM::Matrix4x4 m_CameraInvViewMatrix;
		HM::Matrix4x4 m_CameraInvProjMatrix;
		HM::Matrix4x4 m_CameraInvViewProjMatrix;

		HM::Vector3 m_CameraPosition;

		float m_DeltaTime;
		uint32_t m_BackBufferIndex;

	};

}


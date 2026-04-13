#include "FrameContext.hpp"
#include "HedgehogCommon/Camera/Camera.hpp"

namespace Context
{
	void FrameContext::UpdateContext(const Camera& camera)
	{
		m_CameraViewMatrix = camera.GetViewMatrix();
		m_CameraProjMatrix = camera.GetProjectionMatrix();
		m_CameraViewProjMatrix = m_CameraProjMatrix * m_CameraViewMatrix;

		bool succes = true;
		m_CameraInvViewMatrix = m_CameraViewMatrix.Inverse(succes);
		m_CameraInvProjMatrix = m_CameraProjMatrix.Inverse(succes);
		m_CameraInvViewProjMatrix = m_CameraViewProjMatrix.Inverse(succes);

		m_CameraPosition = camera.GetPosition();

	}

	HM::Matrix4x4 FrameContext::GetCameraViewMatrix() const
	{
		return m_CameraViewMatrix;
	}

	HM::Matrix4x4 FrameContext::GetCameraProjMatrix() const
	{
		return m_CameraProjMatrix;
	}

	HM::Matrix4x4 FrameContext::GetCameraViewProjMatrix() const
	{
		return m_CameraViewProjMatrix;
	}

	HM::Matrix4x4 FrameContext::GetCameraInvViewMatrix() const
	{
		return m_CameraInvViewMatrix;
	}

	HM::Matrix4x4 FrameContext::GetCameraInvProjMatrix() const
	{
		return m_CameraInvProjMatrix;
	}

	HM::Matrix4x4 FrameContext::GetCameraInvViewProjMatrix() const
	{
		return m_CameraInvViewProjMatrix;
	}

	HM::Vector3 FrameContext::GetCameraPosition() const
	{
		return m_CameraPosition;
	}

	float FrameContext::GetDeltaTime() const
	{
		return m_DeltaTime;
	}

	void FrameContext::SetBackBufferIndex(uint32_t index)
	{
		m_BackBufferIndex = index;
	}

	uint32_t FrameContext::GetBackBufferIndex() const
	{
		return m_BackBufferIndex;
	}

}



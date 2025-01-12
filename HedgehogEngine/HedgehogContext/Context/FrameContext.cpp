#include "FrameContext.hpp"
#include "HedgehogCommon/Camera/Camera.hpp"

namespace Context
{
	void FrameContext::UpdateContext(const Camera& camera)
	{
		mCameraViewMatrix = camera.GetViewMatrix();
		mCameraProjMatrix = camera.GetProjectionMatrix();
		mCameraViewProjMatrix = mCameraProjMatrix * mCameraViewMatrix;

		bool succes = true;
		mCameraInvViewMatrix = mCameraViewMatrix.Inverse(succes);
		mCameraInvProjMatrix = mCameraProjMatrix.Inverse(succes);
		mCameraInvViewProjMatrix = mCameraViewProjMatrix.Inverse(succes);

		mCameraPosition = camera.GetPosition();

	}

	HM::Matrix4x4 FrameContext::GetCameraViewMatrix() const
	{
		return mCameraViewMatrix;
	}

	HM::Matrix4x4 FrameContext::GetCameraProjMatrix() const
	{
		return mCameraProjMatrix;
	}

	HM::Matrix4x4 FrameContext::GetCameraViewProjMatrix() const
	{
		return mCameraViewProjMatrix;
	}

	HM::Matrix4x4 FrameContext::GetCameraInvViewMatrix() const
	{
		return mCameraInvViewMatrix;
	}

	HM::Matrix4x4 FrameContext::GetCameraInvProjMatrix() const
	{
		return mCameraInvProjMatrix;
	}

	HM::Matrix4x4 FrameContext::GetCameraInvViewProjMatrix() const
	{
		return mCameraInvViewProjMatrix;
	}

	HM::Vector3 FrameContext::GetCameraPosition() const
	{
		return mCameraPosition;
	}

	float FrameContext::GetDeltaTime() const
	{
		return mDeltaTime;
	}

	void FrameContext::SetBackBufferIndex(uint32_t index)
	{
		mBackBufferIndex = index;
	}

	uint32_t FrameContext::GetBackBufferIndex() const
	{
		return mBackBufferIndex;
	}

}



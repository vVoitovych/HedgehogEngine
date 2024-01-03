#include "FrameContext.hpp"
#include "VulkanEngine/Renderer/Camera/Camera.hpp"

namespace Renderer
{
	void FrameContext::UpdateContext(const Camera& camera)
	{
		mCameraViewMatrix = camera.GetViewMatrix();
		mCameraProjMatrix = camera.GetProjectionMatrix();
		mCameraViewProjMatrix = mCameraProjMatrix * mCameraViewMatrix;

		mCameraInvViewMatrix = inverse(mCameraViewMatrix);
		mCameraInvProjMatrix = inverse(mCameraProjMatrix);
		mCameraInvViewProjMatrix = inverse(mCameraViewProjMatrix);

		mCameraPosition = camera.GetPosition();

	}

	glm::mat4 FrameContext::GetCameraViewMatrix() const
	{
		return mCameraViewMatrix;
	}

	glm::mat4 FrameContext::GetCameraProjMatrix() const
	{
		return mCameraProjMatrix;
	}

	glm::mat4 FrameContext::GetCameraViewProjMatrix() const
	{
		return mCameraViewProjMatrix;
	}

	glm::mat4 FrameContext::GetCameraInvViewMatrix() const
	{
		return mCameraInvViewMatrix;
	}

	glm::mat4 FrameContext::GetCameraInvProjMatrix() const
	{
		return mCameraInvProjMatrix;
	}

	glm::mat4 FrameContext::GetCameraInvViewProjMatrix() const
	{
		return mCameraInvViewProjMatrix;
	}

	glm::vec3 FrameContext::GetCameraPosition() const
	{
		return mCameraPosition;
	}

	float FrameContext::GetDeltaTime() const
	{
		return mDeltaTime;
	}

}



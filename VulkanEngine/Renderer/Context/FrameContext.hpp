#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Renderer
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

		glm::mat4 GetCameraViewMatrix() const;
		glm::mat4 GetCameraProjMatrix() const;
		glm::mat4 GetCameraViewProjMatrix() const;

		glm::mat4 GetCameraInvViewMatrix() const;
		glm::mat4 GetCameraInvProjMatrix() const;
		glm::mat4 GetCameraInvViewProjMatrix() const;

		glm::vec3 GetCameraPosition() const;

		float GetDeltaTime() const;

	private:
		glm::mat4 mCameraViewMatrix;
		glm::mat4 mCameraProjMatrix;
		glm::mat4 mCameraViewProjMatrix;

		glm::mat4 mCameraInvViewMatrix;
		glm::mat4 mCameraInvProjMatrix;
		glm::mat4 mCameraInvViewProjMatrix;

		glm::vec3 mCameraPosition;

		float mDeltaTime;

	};

}


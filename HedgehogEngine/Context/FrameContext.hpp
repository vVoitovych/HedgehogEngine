#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

		glm::mat4 GetCameraViewMatrix() const;
		glm::mat4 GetCameraProjMatrix() const;
		glm::mat4 GetCameraViewProjMatrix() const;

		glm::mat4 GetCameraInvViewMatrix() const;
		glm::mat4 GetCameraInvProjMatrix() const;
		glm::mat4 GetCameraInvViewProjMatrix() const;

		glm::vec3 GetCameraPosition() const;

		float GetDeltaTime() const;

		void SetBackBufferIndex(uint32_t index);
		uint32_t GetBackBufferIndex() const;

	private:
		glm::mat4 mCameraViewMatrix;
		glm::mat4 mCameraProjMatrix;
		glm::mat4 mCameraViewProjMatrix;

		glm::mat4 mCameraInvViewMatrix;
		glm::mat4 mCameraInvProjMatrix;
		glm::mat4 mCameraInvViewProjMatrix;

		glm::vec3 mCameraPosition;

		float mDeltaTime;
		uint32_t mBackBufferIndex;

	};

}


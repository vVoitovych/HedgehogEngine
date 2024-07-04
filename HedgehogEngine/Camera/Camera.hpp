#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace WinManager
{
	struct Controls;
}

namespace Context
{
	enum class CameraType
	{
		OrtoCamera,
		PerspectiveCamera
	};

	class Camera
	{
	public:
		Camera() = default;
		~Camera() = default;

		Camera(const Camera&) = default;
		Camera& operator=(const Camera&) = default;

		void UpdateCamera(float dt, float ratio, const WinManager::Controls& controls);

		void SetFov(float fov);
		void SetAspect(float aspect);
		void SetNearPlane(float nearPlane);
		void SetFarPlane(float farPlane);

		glm::mat4 GetViewMatrix() const;
		glm::mat4 GetProjectionMatrix() const;
		glm::vec3 GetPosition() const;


	private:
		void UpdateMatricies();

	private:
		float mFOV = glm::radians(45.0f);
		float mAspect = 1.0f;
		float mNearPlane = 0.1f;
		float mFarPlane = 1000.0f;

		float mCameraSpeed = 10.0f;

		float mMouseSensitivity = 0.01f;
		float mYaw = 0.0f;
		float mPitch = 0.0f;

		CameraType mCameraType = CameraType::PerspectiveCamera;

		glm::vec3 mPos = glm::vec3(-10.0f, 0.0f, 0.0f);
		glm::vec3 mDirection = glm::vec3(1.0f, 0.0f, 0.0f);
		glm::vec3 mUpVector = glm::vec3(0.0f, 0.0f, 1.0f);
		glm::vec3 mRightVector = glm::normalize(glm::cross(mDirection, mUpVector));

		glm::mat4 mViewMatrix;
		glm::mat4 mProjMatrix;
	};

}



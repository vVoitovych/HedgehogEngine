#pragma once

#include "HedgehogMath/Vector.hpp"
#include "HedgehogMath/Matrix.hpp"
#include "HedgehogMath/Common.hpp"

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

		HM::Matrix4x4 GetViewMatrix() const;
		HM::Matrix4x4 GetProjectionMatrix() const;
		HM::Vector3 GetPosition() const;


	private:
		void UpdateMatricies();

	private:
		float mFOV = HM::ToRadians(45.0f);
		float mAspect = 1.0f;
		float mNearPlane = 0.1f;
		float mFarPlane = 1000.0f;

		float mCameraSpeed = 10.0f;

		float mMouseSensitivity = 0.01f;
		float mYaw = 0.0f;
		float mPitch = 0.0f;

		CameraType mCameraType = CameraType::PerspectiveCamera;

		HM::Vector3 mPos = HM::Vector3(-10.0f, 0.0f, 0.0f);
		HM::Vector3 mDirection = HM::Vector3(1.0f, 0.0f, 0.0f);
		HM::Vector3 mUpVector = HM::Vector3(0.0f, 0.0f, 1.0f);
		HM::Vector3 mRightVector = (mDirection.Cross(mUpVector)).Normalize();

		HM::Matrix4x4 mViewMatrix;
		HM::Matrix4x4 mProjMatrix;
	};

}



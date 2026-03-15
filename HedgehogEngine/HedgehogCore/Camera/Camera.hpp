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

		void UpdateCamera(
			float dt, 
			float ratio, 
			const HM::Vector3& posOffset,
			const HM::Vector2& dirOffset);

		void SetFov(float fov);
		void SetAspect(float aspect);
		void SetNearPlane(float nearPlane);
		void SetFarPlane(float farPlane);

		HM::Matrix4x4 GetViewMatrix() const;
		HM::Matrix4x4 GetProjectionMatrix() const;
		HM::Vector3 GetPosition() const;

		float GetNearPlane() const;
		float GetFarPlane() const;

	private:
		void UpdateMatricies();

	private:
		float m_FOV = HM::ToRadians(45.0f);
		float m_Aspect = 1.0f;
		float m_NearPlane = 0.1f;
		float m_FarPlane = 1000.0f;

		float m_CameraSpeed = 10.0f;

		float m_MouseSensitivity = 0.01f;
		float m_Yaw = 0.0f;
		float m_Pitch = 0.0f;

		CameraType m_CameraType = CameraType::PerspectiveCamera;

		HM::Vector3 m_Pos = HM::Vector3(-10.0f, 0.0f, 0.0f);
		HM::Vector3 m_Direction = HM::Vector3(1.0f, 0.0f, 0.0f);
		HM::Vector3 m_UpVector = HM::Vector3(0.0f, 0.0f, 1.0f);
		HM::Vector3 m_RightVector = (Cross(m_Direction, m_UpVector)).Normalize();

		HM::Matrix4x4 m_ViewMatrix;
		HM::Matrix4x4 m_ProjMatrix;
	};

}



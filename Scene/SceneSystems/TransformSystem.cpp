#include "TransformSystem.hpp"

#include "Scene/SceneComponents/TransformComponent.hpp"

#include "HedgehogMath/Vector.hpp"
#include "HedgehogMath/Matrix.hpp"
#include "HedgehogMath/Common.hpp"


namespace Scene
{
	void TransformSystem::Update(ECS::Coordinator& coordinator)
	{
		for (auto const& entity : entities)
		{
			auto& transform = coordinator.GetComponent<TransformComponent>(entity);

			HM::Matrix4x4 translation = HM::Matrix4x4::GetTranslation(transform.mPososition.x(), transform.mPososition.y(), transform.mPososition.z());
			HM::Matrix4x4 xRotation = HM::Matrix4x4::GetRotationX(HM::ToRadians(transform.mRotation.x()));
			HM::Matrix4x4 yRotation = HM::Matrix4x4::GetRotationY(HM::ToRadians(transform.mRotation.y()));
			HM::Matrix4x4 zRotation = HM::Matrix4x4::GetRotationX(HM::ToRadians(transform.mRotation.z()));
			HM::Matrix4x4 scale = HM::Matrix4x4::GetScale(transform.mScale.x(), transform.mScale.y(), transform.mScale.z());
			
			transform.mObjMatrix = scale * zRotation * yRotation * xRotation * translation;

		}
	}
}



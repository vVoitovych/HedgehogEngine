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

			HM::Matrix4x4 matrix = HM::Matrix4x4::GetIdentity();
			matrix = HM::Matrix4x4::Translate(matrix, transform.mPososition);
			matrix = HM::Matrix4x4::Rotate(matrix, HM::ToRadians(transform.mRotation.x()), { 1.0f, 0.0f, 0.0f });
			matrix = HM::Matrix4x4::Rotate(matrix, HM::ToRadians(transform.mRotation.y()), { 0.0f, 1.0f, 0.0f });
			matrix = HM::Matrix4x4::Rotate(matrix, HM::ToRadians(transform.mRotation.z()), { 0.0f, 0.0f, 1.0f });
			matrix = HM::Matrix4x4::Scale(matrix, transform.mScale);
			
			transform.mObjMatrix = matrix;

		}
	}
}



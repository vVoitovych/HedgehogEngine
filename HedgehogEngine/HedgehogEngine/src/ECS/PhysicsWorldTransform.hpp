#pragma once

#include "ECS/api/ECS.hpp"

#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Quaternion.hpp"
#include "HedgehogMath/api/Vector.hpp"

namespace HedgehogEngine
{
    // An entity's transform in the world, composed from the components themselves (the ObjMatrix may
    // be a frame old): its matrix, its rotation, the product of the scales, and whether an ancestor is
    // scaled unevenly (the rotation and scale are then approximate). Shared by PhysicsSystem's bodies
    // and characters.
    struct WorldTransform
    {
        HM::Matrix4x4  Matrix         = HM::Matrix4x4::GetIdentity();
        HM::Quaternion Rotation       = HM::Quaternion::Identity();
        HM::Vector3    Scale          = HM::Vector3(1.0f, 1.0f, 1.0f);
        bool           UnevenAncestor = false;
    };

    WorldTransform ComputeWorldTransform(ECS::ECS& ecs, ECS::Entity entity);
}

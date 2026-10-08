#pragma once

#include "ECS/api/Entity.hpp"

#include "HedgehogMath/api/Vector.hpp"

namespace HedgehogEngine
{
    // Published by PhysicsSystem after each fixed step, in the physics module's deterministic order
    // (Enters before Exits, then by entity). A pair whose entity died before its Enter was published
    // is dropped; an Exit names a destroyed entity's id, so handlers can let go of it. Stop publishes
    // no Exit. A pair with a trigger collider gives trigger events instead of collision events.

    // Two solid colliders began touching: where, and the normal from A towards B.
    struct CollisionEnterEvent
    {
        ECS::Entity EntityA = ECS::INVALID_ENTITY;
        ECS::Entity EntityB = ECS::INVALID_ENTITY;
        HM::Vector3 Point   = HM::Vector3(0.0f, 0.0f, 0.0f);
        HM::Vector3 Normal  = HM::Vector3(0.0f, 0.0f, 0.0f);
    };

    // They stopped touching, or one of them is gone.
    struct CollisionExitEvent
    {
        ECS::Entity EntityA = ECS::INVALID_ENTITY;
        ECS::Entity EntityB = ECS::INVALID_ENTITY;
    };

    // Other entered the trigger collider of Trigger.
    struct TriggerEnterEvent
    {
        ECS::Entity Trigger = ECS::INVALID_ENTITY;
        ECS::Entity Other   = ECS::INVALID_ENTITY;
    };

    struct TriggerExitEvent
    {
        ECS::Entity Trigger = ECS::INVALID_ENTITY;
        ECS::Entity Other   = ECS::INVALID_ENTITY;
    };
}

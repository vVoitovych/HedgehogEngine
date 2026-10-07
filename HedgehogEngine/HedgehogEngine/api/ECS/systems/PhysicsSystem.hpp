#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "HedgehogPhysics/api/BodyHandle.hpp"
#include "HedgehogPhysics/api/PhysicsEvents.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/System.hpp"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace HP
{
    class PhysicsWorld;
}

namespace HedgehogEngine
{
    class EventBus;

    // An entity view of the rigid bodies (RigidBodyComponent + TransformComponent), so the physics
    // system can warn about one without a collider.
    class RigidBodyListSystem : public ECS::System
    {
    };

    // Simulates the colliders (ColliderComponent + TransformComponent) in Play mode through the
    // engine's HP::PhysicsWorld: one body per entity, static without a RigidBodyComponent (or with a
    // static one), else kinematic or dynamic as it says.
    //
    // OnPlayStart starts the world from the physics settings when a collider exists (a scene without
    // physics never starts Jolt) and makes the bodies. Each fixed step, in OnPostFixedUpdate, after
    // every system's OnFixedUpdate (so a force a script added applies in that step): bodies are made
    // for entities that gained a collider and destroyed for ones that lost it or died, the world
    // steps, and every awake dynamic body's pose is written to its entity's local Position and
    // Rotation (through its parent's world transform) with a TransformChangedEvent, so the Transform
    // phase applies it that frame. OnPlayStop destroys every body; the world stays started. Pause
    // runs no fixed steps, so it holds.
    class PhysicsSystem : public ECS::System
    {
    public:
        // Finds the HP::PhysicsWorld and EventBus services; the Settings service is looked up at
        // Play, as it is registered after the systems. Without the world nothing is simulated.
        HEDGEHOG_ENGINE_API void OnRegister(ECS::ECS& ecs) override;
        HEDGEHOG_ENGINE_API void OnUnregister(ECS::ECS& ecs) override;

        ECS::SystemPhase GetPhase() const override { return ECS::SystemPhase::Simulation; }

        HEDGEHOG_ENGINE_API void OnPlayStart(ECS::ECS& ecs) override;
        HEDGEHOG_ENGINE_API void OnPostFixedUpdate(ECS::ECS& ecs, float fixedDeltaTime) override;
        HEDGEHOG_ENGINE_API void OnPlayStop(ECS::ECS& ecs) override;

        // The bodies the system made, failed ones not counted.
        [[nodiscard]] HEDGEHOG_ENGINE_API uint32_t GetBodyCount() const;
        // Whether the world has been started (by the first Play with a collider).
        [[nodiscard]] HEDGEHOG_ENGINE_API bool IsWorldStarted() const;
        // The entity's body, or an invalid handle.
        [[nodiscard]] HEDGEHOG_ENGINE_API HP::BodyHandle GetBody(ECS::Entity entity) const;

    private:
        struct TrackedBody
        {
            uint32_t       Generation = 0;
            HP::BodyHandle Body; // invalid when the world refused it (logged once)
        };

        bool StartWorld(ECS::ECS& ecs);
        void SyncBodies(ECS::ECS& ecs);
        void CreateBody(ECS::ECS& ecs, ECS::Entity entity);
        void DestroyBodies(ECS::ECS& ecs);
        void WarnRigidBodiesWithoutCollider(ECS::ECS& ecs);
        void WriteBack(ECS::ECS& ecs);

        HP::PhysicsWorld* m_World = nullptr;
        EventBus*         m_Bus   = nullptr;

        std::unordered_map<ECS::Entity, TrackedBody> m_Bodies;
        std::unordered_set<ECS::Entity>              m_Present;  // scratch: this step's entities
        std::vector<ECS::Entity>                     m_Gone;     // scratch: entities losing a body
        std::vector<HP::BodyHandle>                  m_Active;   // scratch: the awake bodies
        std::vector<HP::ContactEvent>                m_Contacts; // the last step's contact events
        std::unordered_set<ECS::Entity>              m_WarnedNoCollider; // one warning per entity per Play
        std::unordered_set<ECS::Entity>              m_WarnedScale;      // likewise, a non-uniformly scaled parent
    };
}

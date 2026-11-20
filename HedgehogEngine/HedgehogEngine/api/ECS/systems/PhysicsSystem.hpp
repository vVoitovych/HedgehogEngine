#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "HedgehogPhysics/api/BodyHandle.hpp"
#include "HedgehogPhysics/api/CharacterTypes.hpp"
#include "HedgehogPhysics/api/ConstraintTypes.hpp"
#include "HedgehogPhysics/api/PhysicsEvents.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/System.hpp"

#include "HedgehogMath/api/Quaternion.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>
#include <optional>
#include <string>
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
    class ResourceCatalog;
    struct CharacterControllerComponent;

    // What PhysicsSystem::Raycast reached.
    struct PhysicsRayHit
    {
        ECS::Entity Entity   = ECS::INVALID_ENTITY;
        HM::Vector3 Point    = HM::Vector3(0.0f, 0.0f, 0.0f);
        HM::Vector3 Normal   = HM::Vector3(0.0f, 0.0f, 0.0f);
        float       Distance = 0.0f;
    };

    // An entity view of the rigid bodies (RigidBodyComponent + TransformComponent), so the physics
    // system can warn about one without a collider.
    class RigidBodyListSystem : public ECS::System
    {
    };

    // An entity view of the characters (CharacterControllerComponent + TransformComponent).
    class CharacterControllerListSystem : public ECS::System
    {
    };

    // An entity view of the joints (JointComponent + TransformComponent).
    class JointListSystem : public ECS::System
    {
    };

    // Simulates the colliders (ColliderComponent + TransformComponent) in Play mode through the
    // engine's HP::PhysicsWorld: one body per entity, static without a RigidBodyComponent (or with a
    // static one), else kinematic or dynamic as it says. Every enabled terrain (the TerrainSystem's
    // TerrainComponent + TransformComponent entities, whose file loads) without a collider is a static
    // height-field body too, of its heights at the moment Play starts or the body is made, on its
    // PhysicsLayer with its Friction.
    //
    // OnPlayStart starts the world from the physics settings when a collider exists (a scene without
    // physics never starts Jolt) and makes the bodies. Each fixed step, in OnPostFixedUpdate, after
    // every system's OnFixedUpdate (so a force a script added applies in that step): bodies are made
    // for entities that gained a collider and destroyed for ones that lost it or died, the world
    // steps, and every awake dynamic body's pose is written to its entity's local Position and
    // Rotation (through its parent's world transform) with a TransformChangedEvent, so the Transform
    // phase applies it that frame. OnPlayStop destroys every body; the world stays started. Pause
    // runs no fixed steps, so it holds.
    //
    // Before each step a kinematic body moves to its entity's current world pose (MoveKinematic, so
    // it pushes dynamic bodies on the way), and a static or dynamic body whose entity's local
    // Position or Rotation gameplay changed since the body last wrote it is teleported there. After
    // it the contacts are published as Collision and Trigger events (Events/PhysicsEvents.hpp).
    //
    // Every CharacterControllerComponent entity is a character of the world (an upright capsule at
    // its world position; a character alone starts the world too; a ColliderComponent beside it makes
    // no body, with one warning per Play). After each step every character moves: walking at its
    // SetCharacterMove velocity (horizontal), keeping its vertical speed under gravity times its
    // GravityScale when off the ground, carried by what it stands on, jumping at a CharacterJump speed
    // when on the ground (the request used by that step either way); then its world position is
    // written to its local Position (through its parent) with a TransformChangedEvent. Its rotation is
    // gameplay's. A Position gameplay changed since the last write teleports it.
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

        // ---- Gameplay, by entity. An entity without a body (none, or not in Play) gets nothing done
        // and zeros back. Velocities are world space, per second; forces and torques act over the
        // next step only; impulses change the velocity at once. Forces and impulses move only a
        // dynamic body; velocities also a kinematic one (whose next MoveKinematic overrides them).
        [[nodiscard]] HEDGEHOG_ENGINE_API HM::Vector3 GetLinearVelocity(ECS::Entity entity) const;
        HEDGEHOG_ENGINE_API void                      SetLinearVelocity(ECS::Entity entity, const HM::Vector3& velocity);
        [[nodiscard]] HEDGEHOG_ENGINE_API HM::Vector3 GetAngularVelocity(ECS::Entity entity) const;
        HEDGEHOG_ENGINE_API void                      SetAngularVelocity(ECS::Entity entity, const HM::Vector3& velocity);
        HEDGEHOG_ENGINE_API void                      AddForce(ECS::Entity entity, const HM::Vector3& force);
        HEDGEHOG_ENGINE_API void                      AddImpulse(ECS::Entity entity, const HM::Vector3& impulse);
        HEDGEHOG_ENGINE_API void                      AddTorque(ECS::Entity entity, const HM::Vector3& torque);
        HEDGEHOG_ENGINE_API void                      AddAngularImpulse(ECS::Entity entity, const HM::Vector3& impulse);

        // The nearest collider (triggers skipped) the ray reaches within maxDistance on a layer of
        // layerMask, or nothing; nothing too when the world has not started.
        [[nodiscard]] HEDGEHOG_ENGINE_API std::optional<PhysicsRayHit>
        Raycast(const HM::Vector3& origin, const HM::Vector3& direction, float maxDistance, uint16_t layerMask = 0xffffu) const;

        // The nearest collider (triggers skipped) a sphere, a box (half extents, turned by rotation)
        // or a capsule (radius and height end to end, along its local +Z, turned by rotation) swept
        // from center along direction reaches within maxDistance on a layer of layerMask, as
        // Raycast's hit (distance the shape travelled; one it starts touching at 0); nothing for a
        // bad size, pose, direction or distance, or before the world starts.
        [[nodiscard]] HEDGEHOG_ENGINE_API std::optional<PhysicsRayHit>
        SphereCast(const HM::Vector3& center, float radius, const HM::Vector3& direction, float maxDistance,
                   uint16_t layerMask = 0xffffu) const;
        [[nodiscard]] HEDGEHOG_ENGINE_API std::optional<PhysicsRayHit>
        BoxCast(const HM::Vector3& center, const HM::Vector3& halfExtents, const HM::Quaternion& rotation,
                const HM::Vector3& direction, float maxDistance, uint16_t layerMask = 0xffffu) const;
        [[nodiscard]] HEDGEHOG_ENGINE_API std::optional<PhysicsRayHit>
        CapsuleCast(const HM::Vector3& center, float radius, float height, const HM::Quaternion& rotation,
                    const HM::Vector3& direction, float maxDistance, uint16_t layerMask = 0xffffu) const;

        // Replaces out with every entity whose collider (triggers only with includeTriggers) touches
        // the shape on a layer of layerMask, once each, by entity id; empty for a bad size or pose, or
        // before the world starts.
        HEDGEHOG_ENGINE_API void OverlapSphere(const HM::Vector3& center, float radius, uint16_t layerMask,
                                               bool includeTriggers, std::vector<ECS::Entity>& out) const;
        HEDGEHOG_ENGINE_API void OverlapBox(const HM::Vector3& center, const HM::Vector3& halfExtents,
                                            const HM::Quaternion& rotation, uint16_t layerMask, bool includeTriggers,
                                            std::vector<ECS::Entity>& out) const;
        HEDGEHOG_ENGINE_API void OverlapCapsule(const HM::Vector3& center, float radius, float height,
                                                const HM::Quaternion& rotation, uint16_t layerMask, bool includeTriggers,
                                                std::vector<ECS::Entity>& out) const;

        // The running world's gravity; zero before it starts. Setting it lasts until the next Play,
        // which reads the physics settings again. Nothing before the world starts.
        [[nodiscard]] HEDGEHOG_ENGINE_API HM::Vector3 GetGravity() const;
        HEDGEHOG_ENGINE_API void                      SetGravity(const HM::Vector3& gravity);

        // Makes the entity's body again from its components (a collider or rigid body edited during
        // Play), keeping its velocities. Nothing for an entity without a body.
        HEDGEHOG_ENGINE_API void RebuildBody(ECS::ECS& ecs, ECS::Entity entity);

        // ---- Characters, by entity: nothing done and zeros back for an entity without one.
        // The walk velocity (world space; its +Z part ignored) kept until changed, and a jump speed
        // for the next step.
        HEDGEHOG_ENGINE_API void SetCharacterMove(ECS::Entity entity, const HM::Vector3& velocity);
        HEDGEHOG_ENGINE_API void CharacterJump(ECS::Entity entity, float speed);
        // On ground it can walk on, after its last move.
        [[nodiscard]] HEDGEHOG_ENGINE_API bool        IsCharacterGrounded(ECS::Entity entity) const;
        [[nodiscard]] HEDGEHOG_ENGINE_API HM::Vector3 GetCharacterVelocity(ECS::Entity entity) const;
        [[nodiscard]] HEDGEHOG_ENGINE_API HM::Vector3 GetCharacterGroundNormal(ECS::Entity entity) const;
        [[nodiscard]] HEDGEHOG_ENGINE_API HM::Vector3 GetCharacterGroundVelocity(ECS::Entity entity) const;
        // The entity's character, or an invalid handle; the characters the system made.
        [[nodiscard]] HEDGEHOG_ENGINE_API HP::CharacterHandle GetCharacter(ECS::Entity entity) const;
        [[nodiscard]] HEDGEHOG_ENGINE_API uint32_t            GetCharacterCount() const;
        // Makes the entity's character again from its component (one edited during Play) where its
        // transform puts it. Nothing for an entity without a character.
        HEDGEHOG_ENGINE_API void RebuildCharacter(ECS::ECS& ecs, ECS::Entity entity);

        // ---- Joints (JointComponent): each enabled joint whose entity has a body, joined to its
        // connected entity's body (none: the world), is a constraint, made after the bodies each step
        // and again whenever either body is made again; it goes with either body. A joint whose two
        // sides are both static, or whose entity (or a live connected entity) has no body, makes
        // nothing, with one warning per Play. Stop destroys them before the bodies.

        // Makes the entity's joint again from its component (one edited during Play).
        HEDGEHOG_ENGINE_API void                               RebuildJoint(ECS::ECS& ecs, ECS::Entity entity);
        [[nodiscard]] HEDGEHOG_ENGINE_API HP::ConstraintHandle GetJoint(ECS::Entity entity) const;
        [[nodiscard]] HEDGEHOG_ENGINE_API uint32_t             GetJointCount() const;
        // A hinge joint's turn from where it was made, in degrees; a slider's travel or a distance's
        // length, in metres; 0 for an entity without a joint of that kind.
        [[nodiscard]] HEDGEHOG_ENGINE_API float GetJointAngle(ECS::Entity entity) const;
        [[nodiscard]] HEDGEHOG_ENGINE_API float GetJointPosition(ECS::Entity entity) const;

    private:
        struct TrackedBody
        {
            uint32_t       Generation = 0;
            HP::BodyHandle Body; // invalid when the world refused it (logged once)
            bool           IsTrigger = false;
            bool           IsKinematic = false;
            // The entity's local transform as the body last saw it (made or written back), so a
            // change gameplay made since is a teleport.
            HM::Vector3 Position = HM::Vector3(0.0f, 0.0f, 0.0f);
            HM::Vector3 Rotation = HM::Vector3(0.0f, 0.0f, 0.0f);
        };

        // The entity's live body, or an invalid handle.
        [[nodiscard]] HP::BodyHandle FindBody(ECS::Entity entity) const;
        void                         FollowTransforms(ECS::ECS& ecs, float fixedDeltaTime);
        void                         PublishContacts(ECS::ECS& ecs);

        bool StartWorld(ECS::ECS& ecs);
        void ApplySettings(ECS::ECS& ecs);
        void SyncBodies(ECS::ECS& ecs);
        void CreateBody(ECS::ECS& ecs, ECS::Entity entity);
        void CreateTerrainBody(ECS::ECS& ecs, ECS::Entity entity);
        // The enabled terrains without a collider whose data loads, appended to out.
        void CollectTerrains(ECS::ECS& ecs, std::vector<ECS::Entity>& out);
        // Whether there is anything to simulate: a collider or a terrain.
        bool HasBodiesToMake(ECS::ECS& ecs);
        void DestroyBodies(ECS::ECS& ecs);
        void WarnRigidBodiesWithoutCollider(ECS::ECS& ecs);
        void WriteBack(ECS::ECS& ecs);

        // Characters (PhysicsSystemCharacters.cpp).
        struct TrackedCharacter
        {
            uint32_t            Generation = 0;
            HP::CharacterHandle Character; // invalid when the world refused it (logged once)
            HM::Vector3         Position = HM::Vector3(0.0f, 0.0f, 0.0f); // the local Position last written
        };
        [[nodiscard]] bool HasCharacters() const;
        // Whether the entity is a character's, so its collider makes no body (warned once per Play).
        bool IsCharacterEntity(ECS::ECS& ecs, ECS::Entity entity);
        void SyncCharacters(ECS::ECS& ecs);
        void CreateCharacter(ECS::ECS& ecs, ECS::Entity entity);
        void FollowCharacterTransforms(ECS::ECS& ecs);
        void MoveCharacters(ECS::ECS& ecs, float fixedDeltaTime);
        void DestroyCharacters(ECS::ECS& ecs);
        [[nodiscard]] const CharacterControllerComponent* FindCharacterComponent(ECS::Entity entity) const;

        // Joints (PhysicsSystemJoints.cpp).
        struct TrackedJoint
        {
            uint32_t             Generation = 0;
            HP::ConstraintHandle Constraint;
            ECS::Entity          Connected = ECS::INVALID_ENTITY;
            HP::BodyHandle       BodyA; // the bodies it joins, so a body made again remakes it
            HP::BodyHandle       BodyB;
        };
        void SyncJoints(ECS::ECS& ecs);
        void CreateJoint(ECS::ECS& ecs, ECS::Entity entity);
        void DestroyJoint(ECS::ECS& ecs, ECS::Entity entity);
        void DestroyJoints(ECS::ECS& ecs);
        // A joint that cannot be made, once per entity per Play.
        void WarnJoint(ECS::Entity entity, const std::string& why);

        std::unordered_map<ECS::Entity, TrackedJoint> m_Joints;
        std::vector<ECS::Entity>                      m_GoneJoints;  // scratch
        std::unordered_set<ECS::Entity>               m_WarnedJoints;

        HP::PhysicsWorld* m_World = nullptr;
        EventBus*         m_Bus   = nullptr;
        ResourceCatalog*  m_Catalog = nullptr; // the terrains' heights
        ECS::ECS*         m_Ecs     = nullptr; // for the character calls by entity

        std::unordered_map<ECS::Entity, TrackedCharacter> m_Characters;
        std::vector<ECS::Entity>                          m_GoneCharacters;      // scratch
        std::unordered_set<ECS::Entity>                   m_WarnedColliderCharacter; // one warning per entity per Play

        std::unordered_map<ECS::Entity, TrackedBody> m_Bodies;
        std::unordered_set<ECS::Entity>              m_Present;  // scratch: this step's entities
        std::vector<ECS::Entity>                     m_Gone;     // scratch: entities losing a body
        std::vector<ECS::Entity>                     m_Terrains; // scratch: this step's terrains
        std::vector<HP::BodyHandle>                  m_Active;   // scratch: the awake bodies
        std::vector<HP::ContactEvent>                m_Contacts; // the last step's contact events
        std::unordered_set<ECS::Entity>              m_GoneTriggers; // destroyed since the last publish
        std::unordered_set<ECS::Entity>              m_WarnedNoCollider; // one warning per entity per Play
        std::unordered_set<ECS::Entity>              m_WarnedScale;      // likewise, a non-uniformly scaled parent
    };
}

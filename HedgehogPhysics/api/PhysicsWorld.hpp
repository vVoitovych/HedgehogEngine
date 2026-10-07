#pragma once

#include "HedgehogPhysics/api/BodyHandle.hpp"
#include "HedgehogPhysics/api/HedgehogPhysicsApi.hpp"
#include "HedgehogPhysics/api/PhysicsEvents.hpp"
#include "HedgehogPhysics/api/PhysicsTypes.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace HP
{
    // Jolt's world, its allocator, job system and layer filters: kept opaque, since only
    // HedgehogPhysics' sources include Jolt (module boundary rule 6).
    struct JoltState;

    // A Jolt physics world. Jolt's process-wide setup (its allocator, factory and registered types)
    // is made by the first world to start and undone by the last to shut down, so any number of
    // worlds may live at once and start and stop in any order.
    class PhysicsWorld
    {
    public:
        HEDGEHOG_PHYSICS_API PhysicsWorld();
        HEDGEHOG_PHYSICS_API ~PhysicsWorld();

        PhysicsWorld(const PhysicsWorld&)            = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;

        // Starts the world, shutting down a running one first. False, with one [Physics] error and
        // the world left stopped, for a desc it refuses: no bodies or more than MAX_BODIES_LIMIT, a
        // non-finite gravity, or a negative thread count.
        HEDGEHOG_PHYSICS_API bool Init(const PhysicsWorldDesc& desc);
        // Destroys everything in the world. Safe twice and before Init; the destructor calls it.
        HEDGEHOG_PHYSICS_API void Shutdown();
        [[nodiscard]] HEDGEHOG_PHYSICS_API bool IsInitialized() const;

        // A non-finite gravity is ignored with a warning. Zero when stopped.
        HEDGEHOG_PHYSICS_API void                      SetGravity(const HM::Vector3& gravity);
        [[nodiscard]] HEDGEHOG_PHYSICS_API HM::Vector3 GetGravity() const;

        // Which layers collide from the next step on.
        HEDGEHOG_PHYSICS_API void                          SetCollisionMatrix(const CollisionMatrix& matrix);
        [[nodiscard]] HEDGEHOG_PHYSICS_API CollisionMatrix GetCollisionMatrix() const;

        [[nodiscard]] HEDGEHOG_PHYSICS_API uint32_t GetBodyCount() const;
        [[nodiscard]] HEDGEHOG_PHYSICS_API int32_t  GetWorkerThreadCount() const;

        // ---- Bodies. Every call with an invalid or stale handle (its body destroyed, or made by
        // another world run) does nothing or returns a default, and never asserts.

        // A body of one shape, added to the world (a static one asleep, others awake). An invalid
        // handle, with one [Physics] error naming why, for a size that is not positive and finite,
        // a layer out of range, a pose or scale that is not finite, a dynamic mass that is not
        // positive, a stopped world, or a world holding its maximum of bodies.
        [[nodiscard]] HEDGEHOG_PHYSICS_API BodyHandle CreateBody(const BodyDesc& desc);
        HEDGEHOG_PHYSICS_API void                     DestroyBody(BodyHandle body);
        HEDGEHOG_PHYSICS_API void                     DestroyAllBodies();
        // Whether body is a live body of this world.
        [[nodiscard]] HEDGEHOG_PHYSICS_API bool       IsValid(BodyHandle body) const;

        // The body's origin and rotation (the default pose for an invalid handle).
        [[nodiscard]] HEDGEHOG_PHYSICS_API BodyPose   GetPose(BodyHandle body) const;
        // Teleports the body, waking it (a static one stays asleep).
        HEDGEHOG_PHYSICS_API void                     SetPose(BodyHandle body, const BodyPose& pose);
        // Moves a kinematic body to pose over the next step of dt seconds, so it reaches it at the
        // step's end and pushes dynamic bodies on the way. Nothing for other bodies or a bad dt.
        HEDGEHOG_PHYSICS_API void                     MoveKinematic(BodyHandle body, const BodyPose& pose, float dt);
        [[nodiscard]] HEDGEHOG_PHYSICS_API MotionType GetMotionType(BodyHandle body) const;
        [[nodiscard]] HEDGEHOG_PHYSICS_API uint64_t   GetUserData(BodyHandle body) const;
        // The bodies Jolt simulated in the last step (awake dynamic and kinematic ones), replacing
        // out's contents: the ones whose pose may have changed.
        HEDGEHOG_PHYSICS_API void                     GetActiveBodies(std::vector<BodyHandle>& out) const;

        // ---- Dynamics. Velocities are in metres (and radians) per second, world space. Each call
        // wakes the body; a static or stale body is left alone, and so is a kinematic one for
        // forces, torques and impulses (a kinematic body takes velocities). Non-finite values are
        // ignored.

        [[nodiscard]] HEDGEHOG_PHYSICS_API HM::Vector3 GetLinearVelocity(BodyHandle body) const;
        HEDGEHOG_PHYSICS_API void                      SetLinearVelocity(BodyHandle body, const HM::Vector3& velocity);
        [[nodiscard]] HEDGEHOG_PHYSICS_API HM::Vector3 GetAngularVelocity(BodyHandle body) const;
        HEDGEHOG_PHYSICS_API void                      SetAngularVelocity(BodyHandle body, const HM::Vector3& velocity);
        // Newtons (newton metres for a torque), applied over the next step only: Jolt clears forces
        // after every step, so a lasting force is added before each one.
        HEDGEHOG_PHYSICS_API void AddForce(BodyHandle body, const HM::Vector3& force);
        HEDGEHOG_PHYSICS_API void AddForceAtPoint(BodyHandle body, const HM::Vector3& force, const HM::Vector3& point);
        HEDGEHOG_PHYSICS_API void AddTorque(BodyHandle body, const HM::Vector3& torque);
        // Newton seconds, changing the velocity at once.
        HEDGEHOG_PHYSICS_API void AddImpulse(BodyHandle body, const HM::Vector3& impulse);
        HEDGEHOG_PHYSICS_API void AddImpulseAtPoint(BodyHandle body, const HM::Vector3& impulse, const HM::Vector3& point);
        HEDGEHOG_PHYSICS_API void AddAngularImpulse(BodyHandle body, const HM::Vector3& impulse);
        // A dynamic body's mass in kilograms; 0 for any other body.
        [[nodiscard]] HEDGEHOG_PHYSICS_API float GetMass(BodyHandle body) const;

        // ---- Queries.

        // The nearest body the ray from origin along direction (any length but zero) reaches within
        // maxDistance metres, on a layer whose bit is set in layerMask; sensors only when asked. A
        // ray starting inside a body hits it at distance 0. Nothing for a stopped world, a zero or
        // non-finite direction, or a maxDistance that is not positive and finite.
        [[nodiscard]] HEDGEHOG_PHYSICS_API std::optional<RayHit> CastRay(const HM::Vector3& origin, const HM::Vector3& direction,
                                                                         float maxDistance, uint16_t layerMask = ALL_LAYERS_MASK,
                                                                         bool includeSensors = false) const;

        // ---- Contacts. Every step queues an Enter when two bodies begin touching and an Exit when
        // they part or one is destroyed (in the next step); bodies kept apart by the collision matrix
        // give none.

        // Appends the events queued since the last drain to out, sorted by type, then the user data
        // and handles of A and B, so the order never depends on Jolt's threads. Drain after every
        // step: the queue grows until drained.
        HEDGEHOG_PHYSICS_API void DrainContactEvents(std::vector<ContactEvent>& out);

        // Advances the world by dt seconds in one collision step. False, doing nothing, when
        // stopped or for a dt that is not positive and finite; false too when Jolt ran out of room
        // for its body pairs or contacts (logged).
        HEDGEHOG_PHYSICS_API bool Step(float dt);

    private:
        // A live dynamic body, or with kinematicToo a kinematic one too.
        [[nodiscard]] bool IsMotion(BodyHandle body, bool kinematicToo) const;
        // Turns the contacts Jolt queued during a step into the world's events (after every step).
        void               CollectContacts();

        std::unique_ptr<JoltState> m_State;
    };
}

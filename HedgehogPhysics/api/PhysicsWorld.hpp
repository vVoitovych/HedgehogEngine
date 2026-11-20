#pragma once

#include "HedgehogPhysics/api/BodyHandle.hpp"
#include "HedgehogPhysics/api/CharacterTypes.hpp"
#include "HedgehogPhysics/api/ConstraintTypes.hpp"
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
        // Destroys every body, every character and every constraint.
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
        // The nearest body shape (a box, sphere or capsule; its Scale baked in), posed at pose and swept
        // along direction (any length but zero) for maxDistance metres, touches, with the same layer
        // and sensor rules as CastRay. A shape touching a body where it starts hits it at distance 0.
        // Nothing for a stopped world, a height field, a size or scale that is not positive and
        // finite, a non-finite pose, centre or direction, or a maxDistance that is not positive and
        // finite.
        [[nodiscard]] HEDGEHOG_PHYSICS_API std::optional<ShapeHit> CastShape(const ShapeDesc& shape, const BodyPose& pose,
                                                                             const HM::Vector3& direction, float maxDistance,
                                                                             uint16_t layerMask = ALL_LAYERS_MASK,
                                                                             bool includeSensors = false) const;
        // Replaces out's contents with every body touching shape at pose, once each, sorted by user
        // data then handle, so the order never depends on Jolt's. Empty for what CastShape refuses.
        HEDGEHOG_PHYSICS_API void Overlap(const ShapeDesc& shape, const BodyPose& pose, std::vector<OverlapHit>& out,
                                          uint16_t layerMask = ALL_LAYERS_MASK, bool includeSensors = false) const;

        // ---- Characters (CharacterTypes.hpp). Each has a kinematic inner body of its capsule, with its
        // user data and layer, so rays hit it, sensors report it in the contact events and dynamic
        // bodies collide with it. A character moves only through MoveCharacter, never in Step.

        // An invalid handle, with one [Physics] error naming why, for a size that is not positive and
        // finite, a non-finite pose or centre, a layer out of range, a mass that is not positive, a
        // slope outside (0, 90) degrees, a negative strength, step, stick-to-floor or contact
        // distance, or a stopped world.
        [[nodiscard]] HEDGEHOG_PHYSICS_API CharacterHandle CreateCharacter(const CharacterDesc& desc);
        HEDGEHOG_PHYSICS_API void                          DestroyCharacter(CharacterHandle character);
        [[nodiscard]] HEDGEHOG_PHYSICS_API bool            IsValid(CharacterHandle character) const;
        // Teleports the character (its rotation is about +Z only in effect) and finds its contacts there.
        HEDGEHOG_PHYSICS_API void SetCharacterPose(CharacterHandle character, const BodyPose& pose);
        // Moves the character over dt seconds: walking at velocity's horizontal part (along its ground
        // when on it, never further up a slope too steep), jumping at velocity's +Z part when on the
        // ground, carried by what it stands on, falling under gravity otherwise, climbing steps up to
        // StepHeight and kept to the floor within StickToFloorDistance. Nothing for a stale handle, a
        // dt that is not positive and finite, or a non-finite velocity or gravity.
        HEDGEHOG_PHYSICS_API void MoveCharacter(CharacterHandle character, const HM::Vector3& velocity,
                                                const HM::Vector3& gravity, float dt);
        // Its state after its last move or teleport; the default state for a stale handle.
        [[nodiscard]] HEDGEHOG_PHYSICS_API CharacterState GetCharacterState(CharacterHandle character) const;
        [[nodiscard]] HEDGEHOG_PHYSICS_API size_t         GetCharacterCount() const;

        // ---- Constraints (ConstraintTypes.hpp). Destroying a body destroys its constraints first.

        // Joins the desc's bodies (or a body and the world) as they are now. An invalid handle, with
        // one [Physics] error naming why, for no body or the same body twice, a body that is not a
        // live body of this world, a zero or non-finite axis, a non-finite anchor, limits out of order
        // or out of the type's range, a negative or non-finite spring, or a stopped world.
        [[nodiscard]] HEDGEHOG_PHYSICS_API ConstraintHandle CreateConstraint(const ConstraintDesc& desc);
        HEDGEHOG_PHYSICS_API void                           DestroyConstraint(ConstraintHandle constraint);
        [[nodiscard]] HEDGEHOG_PHYSICS_API bool             IsValid(ConstraintHandle constraint) const;
        [[nodiscard]] HEDGEHOG_PHYSICS_API size_t           GetConstraintCount() const;
        // A hinge's turn from where it was made, in degrees; 0 for any other constraint or a stale one.
        [[nodiscard]] HEDGEHOG_PHYSICS_API float GetConstraintAngle(ConstraintHandle constraint) const;
        // A slider's travel from where it was made along its axis, or a distance constraint's present
        // length, in metres; 0 for any other constraint or a stale one.
        [[nodiscard]] HEDGEHOG_PHYSICS_API float GetConstraintPosition(ConstraintHandle constraint) const;

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
        // Destroys every character (DestroyAllBodies, before the bodies, as each removes its inner body).
        void               DestroyAllCharacters();
        // Destroys every constraint joining body (before it goes), or with an unset body every one.
        void               DestroyConstraintsOf(BodyHandle body);

        std::unique_ptr<JoltState> m_State;
    };
}

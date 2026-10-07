#pragma once

#include "HedgehogPhysics/api/HedgehogPhysicsApi.hpp"

#include "HedgehogMath/api/Quaternion.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <array>
#include <cstdint>

// Plain data describing a physics world. No Jolt type appears in HedgehogPhysics' headers.
namespace HP
{
    // The physics layers a body can be on; which pairs collide is the world's collision matrix.
    inline constexpr uint32_t PHYSICS_LAYER_COUNT = 16;

    // Row i holds the layers layer i collides with, bit j for layer j. Two layers collide only when
    // both rows say so (ShouldCollide), so a one-sided entry never makes a pair collide.
    using CollisionMatrix = std::array<uint16_t, PHYSICS_LAYER_COUNT>;

    // Every layer collides with every layer.
    [[nodiscard]] HEDGEHOG_PHYSICS_API CollisionMatrix MakeFullCollisionMatrix();

    // Whether bodies on layerA and layerB collide: both rows' bits set. False for a layer out of range.
    [[nodiscard]] HEDGEHOG_PHYSICS_API bool ShouldCollide(const CollisionMatrix& matrix, uint32_t layerA, uint32_t layerB);

    // Sets whether layerA and layerB collide, in both rows. A layer out of range changes nothing.
    HEDGEHOG_PHYSICS_API void SetCollides(CollisionMatrix& matrix, uint32_t layerA, uint32_t layerB, bool collide);

    // As many bodies as the ECS has entities, by default.
    inline constexpr uint32_t DEFAULT_MAX_BODIES = 4096;
    // The most bodies Jolt can address (its BodyID index range).
    inline constexpr uint32_t MAX_BODIES_LIMIT = 1u << 23;

    enum class ShapeType : uint8_t
    {
        Box,
        Sphere,
        Capsule, // along the body's local +Z, the world's up when the body is not turned
    };

    // A body's one shape, in the body's local space before its scale.
    struct ShapeDesc
    {
        ShapeType   Type        = ShapeType::Box;
        HM::Vector3 HalfExtents = HM::Vector3(0.5f, 0.5f, 0.5f); // a box's
        float       Radius      = 0.5f;                          // a sphere's or capsule's
        float       HalfHeight  = 0.5f; // a capsule's cylinder, centre to the centre of an end cap
        HM::Vector3 Center      = HM::Vector3(0.0f, 0.0f, 0.0f); // offset from the body's origin
        // The entity's world scale, baked into the shape: exact on a box; a sphere or capsule takes
        // the largest axis (a non-uniform scale is a warning). Negative axes count as positive.
        HM::Vector3 Scale = HM::Vector3(1.0f, 1.0f, 1.0f);
    };

    enum class MotionType : uint8_t
    {
        Static,    // never moves
        Kinematic, // moved by MoveKinematic, pushes dynamic bodies, is not pushed
        Dynamic,   // simulated
    };

    // Where a body's origin is and how it is turned, in world space.
    struct BodyPose
    {
        HM::Vector3    Position = HM::Vector3(0.0f, 0.0f, 0.0f);
        HM::Quaternion Rotation;
    };

    struct BodyDesc
    {
        ShapeDesc  Shape;
        MotionType Motion = MotionType::Dynamic;
        BodyPose   Pose;
        uint32_t   Layer    = 0;     // a physics layer, below PHYSICS_LAYER_COUNT
        bool       IsSensor = false; // detects overlaps, never pushes or is pushed
        float      Mass     = 1.0f;  // kilograms, a dynamic body's; above 0
        float      Friction       = 0.5f;
        float      Restitution    = 0.0f;
        float      LinearDamping  = 0.05f;
        float      AngularDamping = 0.05f;
        float      GravityFactor  = 1.0f;
        uint64_t   UserData       = 0; // the caller's, returned as given (the engine's entity)
    };

    struct PhysicsWorldDesc
    {
        // Metres per second squared. The engine is Z-up.
        HM::Vector3     Gravity       = HM::Vector3(0.0f, 0.0f, -9.81f);
        uint32_t        MaxBodies     = DEFAULT_MAX_BODIES; // 1 to MAX_BODIES_LIMIT
        CollisionMatrix Collisions    = MakeFullCollisionMatrix();
        // Threads Jolt runs its jobs on: 0 runs them on the thread that calls Step (tests and
        // determinism), N > 0 a pool of N threads.
        int32_t         WorkerThreads = 0;
    };
}

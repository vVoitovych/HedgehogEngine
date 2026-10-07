#pragma once

#include "HedgehogPhysics/api/HedgehogPhysicsApi.hpp"

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

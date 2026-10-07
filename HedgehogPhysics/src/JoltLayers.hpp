#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

#include "HedgehogPhysics/api/PhysicsTypes.hpp"

#include <atomic>
#include <cstdint>

namespace HP
{
    // Jolt's object layer of a body: its physics layer and whether it can move, so the broad phase
    // keeps moving and non-moving bodies in separate trees and never tests two non-moving ones.
    [[nodiscard]] JPH::ObjectLayer ToObjectLayer(uint32_t physicsLayer, bool moving);
    [[nodiscard]] uint32_t         GetPhysicsLayer(JPH::ObjectLayer layer);
    [[nodiscard]] bool             IsMovingLayer(JPH::ObjectLayer layer);

    namespace BroadPhaseLayers
    {
        inline constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        inline constexpr JPH::BroadPhaseLayer MOVING(1);
        inline constexpr JPH::uint            COUNT = 2;
    }

    class BroadPhaseLayerMap final : public JPH::BroadPhaseLayerInterface
    {
    public:
        JPH::uint            GetNumBroadPhaseLayers() const override;
        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override;
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override;
#endif
    };

    // Non-moving bodies are never tested against the non-moving tree.
    class ObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
    {
    public:
        bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhaseLayer) const override;
    };

    // The world's collision matrix. Jolt reads it from its worker threads, so each row is atomic:
    // a matrix set between steps is seen whole by the next one.
    class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
    {
    public:
        void            SetMatrix(const CollisionMatrix& matrix);
        CollisionMatrix GetMatrix() const;

        bool ShouldCollide(JPH::ObjectLayer layerA, JPH::ObjectLayer layerB) const override;

    private:
        std::atomic<uint16_t> m_Rows[PHYSICS_LAYER_COUNT] = {};
    };
}

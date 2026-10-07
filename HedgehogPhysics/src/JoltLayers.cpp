#include "JoltLayers.hpp"

namespace HP
{
    JPH::ObjectLayer ToObjectLayer(uint32_t physicsLayer, bool moving)
    {
        return static_cast<JPH::ObjectLayer>(physicsLayer * 2 + (moving ? 1 : 0));
    }

    uint32_t GetPhysicsLayer(JPH::ObjectLayer layer) { return static_cast<uint32_t>(layer) / 2; }

    bool IsMovingLayer(JPH::ObjectLayer layer) { return (layer & 1) != 0; }

    JPH::uint BroadPhaseLayerMap::GetNumBroadPhaseLayers() const { return BroadPhaseLayers::COUNT; }

    JPH::BroadPhaseLayer BroadPhaseLayerMap::GetBroadPhaseLayer(JPH::ObjectLayer layer) const
    {
        return IsMovingLayer(layer) ? BroadPhaseLayers::MOVING : BroadPhaseLayers::NON_MOVING;
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* BroadPhaseLayerMap::GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const
    {
        return layer == BroadPhaseLayers::MOVING ? "Moving" : "NonMoving";
    }
#endif

    bool ObjectVsBroadPhaseFilter::ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhaseLayer) const
    {
        return IsMovingLayer(layer) || broadPhaseLayer == BroadPhaseLayers::MOVING;
    }

    void ObjectLayerPairFilter::SetMatrix(const CollisionMatrix& matrix)
    {
        for (uint32_t row = 0; row < PHYSICS_LAYER_COUNT; ++row)
            m_Rows[row].store(matrix[row], std::memory_order_relaxed);
    }

    CollisionMatrix ObjectLayerPairFilter::GetMatrix() const
    {
        CollisionMatrix matrix{};
        for (uint32_t row = 0; row < PHYSICS_LAYER_COUNT; ++row)
            matrix[row] = m_Rows[row].load(std::memory_order_relaxed);
        return matrix;
    }

    bool ObjectLayerPairFilter::ShouldCollide(JPH::ObjectLayer layerA, JPH::ObjectLayer layerB) const
    {
        // Two non-moving bodies never collide; the broad phase already keeps them apart.
        if (!IsMovingLayer(layerA) && !IsMovingLayer(layerB))
            return false;
        const uint32_t a = GetPhysicsLayer(layerA);
        const uint32_t b = GetPhysicsLayer(layerB);
        if (a >= PHYSICS_LAYER_COUNT || b >= PHYSICS_LAYER_COUNT)
            return false;
        return ((m_Rows[a].load(std::memory_order_relaxed) >> b) & 1u) != 0 &&
               ((m_Rows[b].load(std::memory_order_relaxed) >> a) & 1u) != 0;
    }
}

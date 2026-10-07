#include "HedgehogPhysics/api/PhysicsTypes.hpp"

namespace HP
{
    CollisionMatrix MakeFullCollisionMatrix()
    {
        CollisionMatrix matrix{};
        matrix.fill(static_cast<uint16_t>((1u << PHYSICS_LAYER_COUNT) - 1));
        return matrix;
    }

    bool ShouldCollide(const CollisionMatrix& matrix, uint32_t layerA, uint32_t layerB)
    {
        if (layerA >= PHYSICS_LAYER_COUNT || layerB >= PHYSICS_LAYER_COUNT)
            return false;
        return ((matrix[layerA] >> layerB) & 1u) != 0 && ((matrix[layerB] >> layerA) & 1u) != 0;
    }

    void SetCollides(CollisionMatrix& matrix, uint32_t layerA, uint32_t layerB, bool collide)
    {
        if (layerA >= PHYSICS_LAYER_COUNT || layerB >= PHYSICS_LAYER_COUNT)
            return;
        const auto set = [collide](uint16_t& row, uint32_t bit)
        {
            if (collide)
                row = static_cast<uint16_t>(row | (1u << bit));
            else
                row = static_cast<uint16_t>(row & ~(1u << bit));
        };
        set(matrix[layerA], layerB);
        set(matrix[layerB], layerA);
    }
}

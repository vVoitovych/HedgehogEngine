#include "api/SceneExtractor.hpp"
#include "Profiler/api/Profiler.hpp"

#include "api/RenderScene.hpp"

#include "ECS/api/ECS.hpp"
#include "HedgehogEngine/api/ECS/components/TerrainComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/TerrainSystem.hpp"

namespace HX
{
    void SceneExtractor::ExtractTerrains(const ECS::ECS& ecs, const HedgehogEngine::TerrainSystem& terrainSystem,
                                         RenderScene& outScene, std::span<const HM::AABB> meshLocalBounds) const
    {
        HH_PROFILE_ZONE("Extract: terrains");
        for (const ECS::Entity entity : terrainSystem.GetEntities())
        {
            const auto& terrain = ecs.GetComponent<HedgehogEngine::TerrainComponent>(entity);
            if (!terrain.Enabled || terrain.MeshIndex < 0)
                continue;
            const auto&  transform = ecs.GetComponent<HedgehogEngine::TransformComponent>(entity);
            const size_t meshIndex = static_cast<size_t>(terrain.MeshIndex);
            if (meshIndex >= meshLocalBounds.size())
                continue; // its mesh is not loaded yet

            RenderInstance instance;
            instance.WorldMatrix = transform.ObjMatrix;
            instance.WorldBounds = meshLocalBounds[meshIndex].Transform(transform.ObjMatrix);
            instance.MeshIndex   = meshIndex;
            // ponytail: the scene's first material until terrain shading draws the splat layers.
            instance.MaterialIndex = 0;
            instance.Layer         = terrain.RenderLayer;
            instance.SourceId      = static_cast<uint64_t>(entity);
            outScene.Instances.push_back(instance);
        }
    }
}

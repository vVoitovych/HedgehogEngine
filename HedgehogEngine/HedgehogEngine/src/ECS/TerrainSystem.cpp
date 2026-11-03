#include "HedgehogEngine/api/ECS/systems/TerrainSystem.hpp"

#include "HedgehogEngine/api/Containers/TerrainContainer.hpp"
#include "HedgehogEngine/api/ECS/components/TerrainComponent.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"

namespace HedgehogEngine
{
    void TerrainSystem::OnRegister(ECS::ECS& ecs)
    {
        if (ResourceCatalog* catalog = ecs.GetServices().Find<ResourceCatalog>())
            m_Terrains = &catalog->GetTerrainContainer();
    }

    void TerrainSystem::OnUnregister(ECS::ECS& /*ecs*/)
    {
        m_Terrains = nullptr;
    }

    ECS::SystemPhase TerrainSystem::GetPhase() const
    {
        return ECS::SystemPhase::Sync;
    }

    void TerrainSystem::OnFrame(ECS::ECS& ecs, const ECS::FrameContext& /*ctx*/)
    {
        if (m_Terrains)
            Update(ecs, *m_Terrains);
    }

    void TerrainSystem::Update(ECS::ECS& ecs, TerrainContainer& terrains)
    {
        for (const ECS::Entity entity : m_Entities)
        {
            TerrainComponent& terrain = ecs.GetComponent<TerrainComponent>(entity);
            // A path is resolved once, a file that fails included; a restored or loaded component
            // starts unresolved.
            if (terrain.ResolvedTerrain == terrain.Terrain)
                continue;
            const std::optional<size_t> index = terrains.FindOrLoad(terrain.Terrain);
            terrain.TerrainIndex              = index ? static_cast<int32_t>(*index) : -1;
            terrain.ResolvedTerrain           = terrain.Terrain;
        }
    }
}

#include "HedgehogEngine/api/ECS/systems/TerrainSystem.hpp"

#include "HedgehogEngine/api/Containers/TerrainContainer.hpp"
#include "HedgehogEngine/api/ECS/components/TerrainComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"

#include <algorithm>

namespace HedgehogEngine
{
    void TerrainSystem::OnRegister(ECS::ECS& ecs)
    {
        m_Catalog = ecs.GetServices().Find<ResourceCatalog>();
    }

    void TerrainSystem::OnUnregister(ECS::ECS& /*ecs*/)
    {
        m_Catalog = nullptr;
    }

    ECS::SystemPhase TerrainSystem::GetPhase() const
    {
        return ECS::SystemPhase::Sync;
    }

    void TerrainSystem::OnFrame(ECS::ECS& ecs, const ECS::FrameContext& /*ctx*/)
    {
        if (!m_Catalog || !ecs.HasSystem<MeshSystem>())
            return;
        Update(ecs, m_Catalog->GetTerrainContainer());
        UpdateMeshes(ecs, *ecs.GetSystem<MeshSystem>(), *m_Catalog);
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
            terrain.MeshIndex                 = -1;
            terrain.ResolvedTerrain           = terrain.Terrain;
        }
    }

    void TerrainSystem::UpdateMeshes(ECS::ECS& ecs, MeshSystem& meshes, ResourceCatalog& catalog)
    {
        const TerrainContainer& terrains = catalog.GetTerrainContainer();
        for (const ECS::Entity entity : m_Entities)
        {
            TerrainComponent& terrain = ecs.GetComponent<TerrainComponent>(entity);
            if (terrain.TerrainIndex < 0)
                continue;
            // The mesh of the terrain's resolution: one given another resolution is listed again.
            const size_t                    index = static_cast<size_t>(terrain.TerrainIndex);
            const std::string               path  = MakeTerrainMeshPath(terrains.GetPath(index), terrains.GetTerrain(index).Resolution);
            const std::vector<std::string>& paths = meshes.GetMeshes();
            if (terrain.MeshIndex >= 0 && static_cast<size_t>(terrain.MeshIndex) < paths.size() &&
                paths[static_cast<size_t>(terrain.MeshIndex)] == path)
                continue;
            const auto                      found = std::find(paths.begin(), paths.end(), path);
            terrain.MeshIndex                     = static_cast<int32_t>(found - paths.begin());
            if (found == paths.end())
                meshes.AddMeshPath(path);
        }
        catalog.UpdateMeshes(meshes);
    }
}

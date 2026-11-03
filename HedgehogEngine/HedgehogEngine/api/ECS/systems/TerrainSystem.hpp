#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/System.hpp"

namespace HedgehogEngine
{
    class MeshSystem;
    class ResourceCatalog;
    class TerrainContainer;

    // The terrains (TerrainComponent + TransformComponent). In the Sync phase (after MeshSystem) it
    // resolves each terrain's TerrainIndex from its Terrain path through the ResourceCatalog
    // service's TerrainContainer, loading files on first use, then lists each terrain's mesh with the
    // MeshSystem and has the catalog build it (nothing without the catalog service or MeshSystem).
    class TerrainSystem : public ECS::System
    {
    public:
        HEDGEHOG_ENGINE_API void OnRegister(ECS::ECS& ecs) override;
        HEDGEHOG_ENGINE_API void OnUnregister(ECS::ECS& ecs) override;
        [[nodiscard]] HEDGEHOG_ENGINE_API ECS::SystemPhase GetPhase() const override;
        HEDGEHOG_ENGINE_API void OnFrame(ECS::ECS& ecs, const ECS::FrameContext& ctx) override;

        // Resolves every terrain whose Terrain path changed since it was last resolved.
        HEDGEHOG_ENGINE_API void Update(ECS::ECS& ecs, TerrainContainer& terrains);
        // Lists the mesh of every resolved terrain without a MeshIndex, then has the catalog load
        // new meshes and rebuild sculpted ones (ResourceCatalog::UpdateMeshes).
        HEDGEHOG_ENGINE_API void UpdateMeshes(ECS::ECS& ecs, MeshSystem& meshes, ResourceCatalog& catalog);

    private:
        ResourceCatalog* m_Catalog = nullptr;
    };
}

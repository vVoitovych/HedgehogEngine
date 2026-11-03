#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/System.hpp"

namespace HedgehogEngine
{
    class TerrainContainer;

    // The terrains (TerrainComponent + TransformComponent). In the Sync phase it resolves each
    // terrain's TerrainIndex from its Terrain path through the ResourceCatalog service's
    // TerrainContainer (found in OnRegister; nothing without one), loading files on first use.
    class TerrainSystem : public ECS::System
    {
    public:
        HEDGEHOG_ENGINE_API void OnRegister(ECS::ECS& ecs) override;
        HEDGEHOG_ENGINE_API void OnUnregister(ECS::ECS& ecs) override;
        [[nodiscard]] HEDGEHOG_ENGINE_API ECS::SystemPhase GetPhase() const override;
        HEDGEHOG_ENGINE_API void OnFrame(ECS::ECS& ecs, const ECS::FrameContext& ctx) override;

        // Resolves every terrain whose Terrain path changed since it was last resolved.
        HEDGEHOG_ENGINE_API void Update(ECS::ECS& ecs, TerrainContainer& terrains);

    private:
        TerrainContainer* m_Terrains = nullptr;
    };
}

#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/System.hpp"
#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"

#include <vector>
#include <string>

namespace HedgehogEngine
{
    class ResourceCatalog;

    // Lists the material files RenderComponents name. In the Sync phase it has the ResourceCatalog
    // service (found in OnRegister; nothing without one) load the ones listed since the last frame.
    class RenderSystem : public ECS::System
    {
    public:
        HEDGEHOG_ENGINE_API void OnRegister(ECS::ECS& ecs) override;
        HEDGEHOG_ENGINE_API void OnUnregister(ECS::ECS& ecs) override;
        [[nodiscard]] HEDGEHOG_ENGINE_API ECS::SystemPhase GetPhase() const override;
        HEDGEHOG_ENGINE_API void OnFrame(ECS::ECS& ecs, const ECS::FrameContext& ctx) override;

        HEDGEHOG_ENGINE_API void   Update(ECS::ECS& ecs, ECS::Entity entity);
        HEDGEHOG_ENGINE_API void   UpdateSystem(ECS::ECS& ecs);
        HEDGEHOG_ENGINE_API size_t GetMaterialsCount() const;
        HEDGEHOG_ENGINE_API const std::vector<std::string>&  GetMaterials() const;
        HEDGEHOG_ENGINE_API RenderComponent& GetRenderComponentByIndex(ECS::ECS& ecs, size_t index) const;
        HEDGEHOG_ENGINE_API const std::vector<ECS::Entity>&  GetEntities() const;

    private:
        void UpdateMaterialPath(ECS::ECS& ecs, ECS::Entity entity);

    private:
        std::vector<std::string> m_MaterialPaths;
        ResourceCatalog*         m_Catalog = nullptr;
    };
}

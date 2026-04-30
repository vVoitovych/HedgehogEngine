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
    class RenderSystem : public ECS::System
    {
    public:
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
    };
}

#pragma once

#include "ECS/api/System.hpp"
#include "ECS/api/ECS.hpp"

#include "Scene/SceneComponents/RenderComponent.hpp"

#include <vector>
#include <string>

namespace Scene
{
    class RenderSystem : public ECS::System
    {
    public:
        void Update(ECS::ECS& ecs, ECS::Entity entity);
        void UpdateSystem(ECS::ECS& ecs);
        size_t GetMaterialsCount() const;
        const std::vector<std::string>& GetMaterials() const;

        RenderComponent& GetRenderComponentByIndex(ECS::ECS& ecs, size_t index) const;

        const std::vector<ECS::Entity>& GetEntities() const;

    private:
        void UpdateMaterialPath(ECS::ECS& ecs, ECS::Entity entity);

    private:
        std::vector<std::string> m_MaterialPaths;
    };
}

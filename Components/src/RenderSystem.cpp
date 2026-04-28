#include "Components/api/RenderSystem.hpp"

#include <algorithm>

namespace Scene
{
    void RenderSystem::Update(ECS::ECS& ecs, ECS::Entity entity)
    {
        auto& render = ecs.GetComponent<RenderComponent>(entity);
        if (render.m_MaterialIndex.has_value())
        {
            if (m_MaterialPaths[render.m_MaterialIndex.value()] != render.m_Material)
            {
                UpdateMaterialPath(ecs, entity);
            }
        }
        else
        {
            UpdateMaterialPath(ecs, entity);
        }
    }

    void RenderSystem::UpdateSystem(ECS::ECS& ecs)
    {
        for (auto entity : m_Entities)
        {
            Update(ecs, entity);
        }
    }

    size_t RenderSystem::GetMaterialsCount() const
    {
        return m_MaterialPaths.size();
    }

    const std::vector<std::string>& RenderSystem::GetMaterials() const
    {
        return m_MaterialPaths;
    }

    const std::vector<ECS::Entity>& RenderSystem::GetEntities() const
    {
        return m_Entities;
    }

    RenderComponent& RenderSystem::GetRenderComponentByIndex(ECS::ECS& ecs, size_t index) const
    {
        auto entity = m_Entities[index];
        return ecs.GetComponent<RenderComponent>(entity);
    }

    void RenderSystem::UpdateMaterialPath(ECS::ECS& ecs, ECS::Entity entity)
    {
        auto& component = ecs.GetComponent<RenderComponent>(entity);

        if (!component.m_Material.empty())
        {
            auto it = std::find(m_MaterialPaths.begin(), m_MaterialPaths.end(), component.m_Material);
            if (it != m_MaterialPaths.end())
            {
                component.m_MaterialIndex = static_cast<uint64_t>(it - m_MaterialPaths.begin());
            }
            else
            {
                component.m_MaterialIndex = m_MaterialPaths.size();
                m_MaterialPaths.push_back(component.m_Material);
            }
        }
    }
}

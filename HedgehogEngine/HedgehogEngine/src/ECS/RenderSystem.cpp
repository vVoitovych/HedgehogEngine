#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"

#include <algorithm>

namespace HedgehogEngine
{
    std::string NormalizeMaterialPath(std::string_view path)
    {
        constexpr std::string_view ASSETS_PREFIX = "assets://";
        if (path.starts_with(ASSETS_PREFIX))
            path.remove_prefix(ASSETS_PREFIX.size());

        std::string normalized;
        normalized.reserve(path.size());
        size_t start = 0;
        while (start <= path.size())
        {
            size_t end = path.find_first_of("/\\", start);
            if (end == std::string_view::npos)
                end = path.size();
            const std::string_view segment = path.substr(start, end - start);
            if (!segment.empty() && segment != ".")
            {
                if (!normalized.empty())
                    normalized += '/';
                normalized += segment;
            }
            start = end + 1;
        }
        return normalized;
    }

    void RenderSystem::OnRegister(ECS::ECS& ecs)
    {
        m_Catalog = ecs.GetServices().Find<ResourceCatalog>();
    }

    void RenderSystem::OnUnregister(ECS::ECS& /*ecs*/)
    {
        m_Catalog = nullptr;
    }

    ECS::SystemPhase RenderSystem::GetPhase() const
    {
        return ECS::SystemPhase::Sync;
    }

    void RenderSystem::OnFrame(ECS::ECS& /*ecs*/, const ECS::FrameContext& /*ctx*/)
    {
        if (m_Catalog)
            m_Catalog->UpdateMaterials(*this);
    }

    void RenderSystem::Update(ECS::ECS& ecs, ECS::Entity entity)
    {
        auto& render = ecs.GetComponent<RenderComponent>(entity);
        if (render.MaterialIndex.has_value())
        {
            if (m_MaterialPaths[render.MaterialIndex.value()] != NormalizeMaterialPath(render.Material))
                UpdateMaterialPath(ecs, entity);
        }
        else
        {
            UpdateMaterialPath(ecs, entity);
        }
    }

    void RenderSystem::UpdateSystem(ECS::ECS& ecs)
    {
        for (auto entity : m_Entities)
            Update(ecs, entity);
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
        return ecs.GetComponent<RenderComponent>(m_Entities[index]);
    }

    void RenderSystem::UpdateMaterialPath(ECS::ECS& ecs, ECS::Entity entity)
    {
        auto& component = ecs.GetComponent<RenderComponent>(entity);

        if (!component.Material.empty())
        {
            std::string key = NormalizeMaterialPath(component.Material);
            auto        it  = std::find(m_MaterialPaths.begin(), m_MaterialPaths.end(), key);
            if (it != m_MaterialPaths.end())
            {
                component.MaterialIndex = static_cast<uint64_t>(it - m_MaterialPaths.begin());
            }
            else
            {
                component.MaterialIndex = m_MaterialPaths.size();
                m_MaterialPaths.push_back(std::move(key));
            }
        }
    }
}

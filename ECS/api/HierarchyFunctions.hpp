#pragma once

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include <string>
#include <vector>

namespace ECS
{
    // Note: inside namespace ECS, the unqualified name 'ECS' refers to class ECS::ECS.

    inline const std::string& GetName(const ECS& ecs, Entity entity)
    {
        return ecs.GetComponent<HierarchyComponent>(entity).m_Name;
    }

    inline Entity GetParent(const ECS& ecs, Entity entity)
    {
        return ecs.GetComponent<HierarchyComponent>(entity).m_Parent;
    }

    inline const std::vector<Entity>& GetChildren(const ECS& ecs, Entity entity)
    {
        return ecs.GetComponent<HierarchyComponent>(entity).m_Children;
    }

    inline void SetName(ECS& ecs, Entity entity, const std::string& name)
    {
        ecs.GetComponent<HierarchyComponent>(entity).m_Name = name;
    }

    inline void SetParent(ECS& ecs, Entity entity, Entity parent)
    {
        ecs.GetComponent<HierarchyComponent>(entity).m_Parent = parent;
    }

    inline void AddChild(ECS& ecs, Entity entity, Entity child)
    {
        ecs.GetComponent<HierarchyComponent>(entity).m_Children.push_back(child);
    }
}

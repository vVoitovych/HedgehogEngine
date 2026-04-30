#include "HedgehogEngine/api/ECS/GameObjectHelpers.hpp"
#include "HedgehogEngine/api/ECS/systems/HierarchySystem.hpp"

#include <algorithm>
#include <sstream>

namespace HedgehogEngine
{
    ECS::Entity CreateSceneRoot(ECS::ECS& ecs, HierarchySystem& hierarchySystem)
    {
        ECS::Entity root = ecs.CreateEntity();
        ecs.AddComponent(root, TransformComponent{});
        ecs.AddComponent(root, ECS::HierarchyComponent{ "Root", root, {} });
        hierarchySystem.SetRoot(root);
        return root;
    }

    ECS::Entity CreateGameObject(ECS::ECS& ecs, ECS::Entity root,
                                  std::optional<ECS::Entity> parent)
    {
        ECS::Entity realParent = parent.value_or(root);

        auto& parentHierarchy = ecs.GetComponent<ECS::HierarchyComponent>(realParent);
        ECS::Entity entity    = ecs.CreateEntity();
        ecs.AddComponent(entity, TransformComponent{});
        ecs.AddComponent(entity, ECS::HierarchyComponent{ GetUniqueGameObjectName(), realParent, {} });
        parentHierarchy.m_Children.push_back(entity);

        return entity;
    }

    void DeleteGameObject(ECS::ECS& ecs, ECS::Entity entity)
    {
        auto& hierarchy       = ecs.GetComponent<ECS::HierarchyComponent>(entity);
        auto& parentHierarchy = ecs.GetComponent<ECS::HierarchyComponent>(hierarchy.m_Parent);

        auto it = std::find(parentHierarchy.m_Children.begin(),
                             parentHierarchy.m_Children.end(), entity);
        if (it != parentHierarchy.m_Children.end())
        {
            parentHierarchy.m_Children.erase(it);
            for (ECS::Entity child : hierarchy.m_Children)
            {
                auto& childHierarchy    = ecs.GetComponent<ECS::HierarchyComponent>(child);
                childHierarchy.m_Parent = hierarchy.m_Parent;
                parentHierarchy.m_Children.push_back(child);
            }
        }
        ecs.DestroyEntity(entity);
    }

    void DeleteGameObjectAndChildren(ECS::ECS& ecs, ECS::Entity entity)
    {
        auto& hierarchy = ecs.GetComponent<ECS::HierarchyComponent>(entity);
        for (ECS::Entity child : hierarchy.m_Children)
            DeleteGameObjectAndChildren(ecs, child);
        ecs.DestroyEntity(entity);
    }

    std::string GetUniqueGameObjectName()
    {
        static size_t s_Index = 0;
        std::ostringstream ss;
        ss << "GameObject_" << s_Index++;
        return ss.str();
    }
}

#include "HedgehogEngine/api/ECS/GameObjectHelpers.hpp"
#include "HedgehogEngine/api/ECS/systems/HierarchySystem.hpp"

#include <algorithm>
#include <sstream>

namespace Components
{
    ECS::Entity CreateSceneRoot(ECS::ECS& ecs, Scene::HierarchySystem& hierarchySystem)
    {
        ECS::Entity root = ecs.CreateEntity();
        ecs.AddComponent(root, Scene::TransformComponent{});
        ecs.AddComponent(root, Scene::HierarchyComponent{ "Root", root, {} });
        hierarchySystem.SetRoot(root);
        return root;
    }

    ECS::Entity CreateGameObject(ECS::ECS& ecs, ECS::Entity root,
                                  std::optional<ECS::Entity> parent)
    {
        ECS::Entity realParent = parent.value_or(root);

        auto& parentHierarchy = ecs.GetComponent<Scene::HierarchyComponent>(realParent);
        ECS::Entity entity    = ecs.CreateEntity();
        ecs.AddComponent(entity, Scene::TransformComponent{});
        ecs.AddComponent(entity, Scene::HierarchyComponent{ GetUniqueGameObjectName(), realParent, {} });
        parentHierarchy.m_Children.push_back(entity);

        return entity;
    }

    void DeleteGameObject(ECS::ECS& ecs, ECS::Entity entity)
    {
        auto& hierarchy       = ecs.GetComponent<Scene::HierarchyComponent>(entity);
        auto& parentHierarchy = ecs.GetComponent<Scene::HierarchyComponent>(hierarchy.m_Parent);

        auto it = std::find(parentHierarchy.m_Children.begin(),
                             parentHierarchy.m_Children.end(), entity);
        if (it != parentHierarchy.m_Children.end())
        {
            parentHierarchy.m_Children.erase(it);
            for (ECS::Entity child : hierarchy.m_Children)
            {
                auto& childHierarchy    = ecs.GetComponent<Scene::HierarchyComponent>(child);
                childHierarchy.m_Parent = hierarchy.m_Parent;
                parentHierarchy.m_Children.push_back(child);
            }
        }
        ecs.DestroyEntity(entity);
    }

    void DeleteGameObjectAndChildren(ECS::ECS& ecs, ECS::Entity entity)
    {
        auto& hierarchy = ecs.GetComponent<Scene::HierarchyComponent>(entity);
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

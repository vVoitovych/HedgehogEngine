#include "api/ECS.hpp"

#include <cassert>

namespace ECS
{
    void ECS::Init()
    {
        m_ComponentManager = std::make_unique<ComponentManager>();
        m_EntityManager    = std::make_unique<EntityManager>();
        m_SystemManager    = std::make_unique<SystemManager>();
        m_RootEntity       = INVALID_ENTITY;
    }

    Entity ECS::CreateEntity()
    {
        return m_EntityManager->CreateEntity();
    }

    void ECS::CreateEntity(Entity entity)
    {
        m_EntityManager->CreateEntity(entity);
    }

    void ECS::DestroyEntity(Entity entity)
    {
        if (entity == m_RootEntity)
            m_RootEntity = INVALID_ENTITY;

        // Components go first, so removal callbacks still see a live entity.
        m_ComponentManager->EntityDestroyed(entity);
        m_SystemManager->EntityDestroyed(entity);
        m_EntityManager->DestroyEntity(entity);
    }

    Entity ECS::GetRoot() const
    {
        return m_RootEntity;
    }

    void ECS::SetRoot(Entity entity)
    {
        m_RootEntity = entity;
    }

    bool ECS::IsAlive(Entity entity) const
    {
        return m_EntityManager->IsAlive(entity);
    }

    uint32_t ECS::GetGeneration(Entity entity) const
    {
        return m_EntityManager->GetGeneration(entity);
    }
}

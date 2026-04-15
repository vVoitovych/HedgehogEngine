#include "api/ECS.hpp"

namespace ECS
{
    void ECS::Init()
    {
        m_ComponentManager = std::make_unique<ComponentManager>();
        m_EntityManager    = std::make_unique<EntityManager>();
        m_SystemManager    = std::make_unique<SystemManager>();
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
        m_EntityManager->DestroyEntity(entity);
        m_ComponentManager->EntityDestroyed(entity);
        m_SystemManager->EntityDestroyed(entity);
    }
}

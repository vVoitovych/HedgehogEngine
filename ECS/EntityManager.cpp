#include "EntityManager.hpp"
#include <cassert>
#include <algorithm>

namespace ECS
{
    EntityManager::EntityManager()
        : m_EntityCount(0)
    {
        for (int i = static_cast<int>(MAX_ENTITIES) - 1; i >= 0; --i)
        {
            m_EntityPool.push_back(static_cast<size_t>(i));
        }
    }

    Entity EntityManager::CreateEntity()
    {
        assert(m_EntityCount < MAX_ENTITIES && "Too many entities.");

        const Entity result = m_EntityPool.back();
        m_EntityPool.pop_back();
        ++m_EntityCount;

        return result;
    }

    void EntityManager::CreateEntity(Entity entity)
    {
        assert(m_EntityCount < MAX_ENTITIES && "Too many entities.");
        auto it = std::find(std::begin(m_EntityPool), std::end(m_EntityPool), entity);
        assert(it != std::end(m_EntityPool) && "Entity already created.");
        ++m_EntityCount;
        m_EntityPool.erase(it);
    }

    void EntityManager::DestroyEntity(Entity entity)
    {
        assert(entity < MAX_ENTITIES && "Entity out of range.");
        m_Signatures[entity].reset();
        m_EntityPool.push_back(entity);
        --m_EntityCount;
    }

    Signature EntityManager::GetSignature(Entity entity) const
    {
        assert(entity < MAX_ENTITIES && "Entity out of range.");
        return m_Signatures[entity];
    }

    void EntityManager::SetSignature(Entity entity, Signature signature)
    {
        assert(entity < MAX_ENTITIES && "Entity out of range.");
        m_Signatures[entity] = signature;
    }
}

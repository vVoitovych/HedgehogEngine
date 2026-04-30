#pragma once

#include <memory>

#include "EcsApi.hpp"
#include "Entity.hpp"
#include "System.hpp"
#include "ComponentManager.hpp"
#include "EntityManager.hpp"
#include "SystemManager.hpp"

namespace ECS
{
    class ECS
    {
    public:
        ECS_API void Init();

        ECS_API Entity CreateEntity();
        ECS_API void   CreateEntity(Entity entity);
        ECS_API void   DestroyEntity(Entity entity);

        ECS_API Entity GetRoot()             const;
        ECS_API void   SetRoot(Entity entity);

        template<typename T>
        void RegisterComponent()
        {
            m_ComponentManager->RegisterComponent<T>();
        }

        template<typename T>
        void AddComponent(Entity entity, T component)
        {
            m_ComponentManager->AddComponent<T>(entity, component);

            auto signature = m_EntityManager->GetSignature(entity);
            signature.set(m_ComponentManager->GetComponentType<T>(), true);
            m_EntityManager->SetSignature(entity, signature);

            m_SystemManager->EntityChangedSignature(entity, signature);
        }

        template<typename T>
        void RemoveComponent(Entity entity)
        {
            m_ComponentManager->RemoveComponent<T>(entity);

            auto signature = m_EntityManager->GetSignature(entity);
            signature.set(m_ComponentManager->GetComponentType<T>(), false);
            m_EntityManager->SetSignature(entity, signature);

            m_SystemManager->EntityChangedSignature(entity, signature);
        }

        template<typename T>
        T& GetComponent(Entity entity) const
        {
            return m_ComponentManager->GetComponent<T>(entity);
        }

        template<typename T>
        bool HasComponent(Entity entity) const
        {
            return m_ComponentManager->HasComponent<T>(entity);
        }

        template<typename T>
        ComponentType GetComponentType() const
        {
            return m_ComponentManager->GetComponentType<T>();
        }

        template<typename T>
        std::shared_ptr<T> RegisterSystem()
        {
            return m_SystemManager->RegisterSystem<T>();
        }

        template<typename T>
        void SetSystemSignature(Signature signature)
        {
            m_SystemManager->SetSignature<T>(signature);
        }

    private:
        std::unique_ptr<ComponentManager> m_ComponentManager;
        std::unique_ptr<EntityManager>    m_EntityManager;
        std::unique_ptr<SystemManager>    m_SystemManager;

        Entity m_RootEntity{INVALID_ENTITY};
    };
}

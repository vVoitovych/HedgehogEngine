#pragma once

#include <memory>

#include "EcsApi.hpp"
#include "ComponentManager.hpp"
#include "EntityManager.hpp"
#include "SystemManager.hpp"

namespace ECS
{
#pragma warning(push)
#pragma warning(disable: 4251) // unique_ptr members of non-exported template types are safe across DLL boundary
    class ECS_API ECS
    {
    public:
        void Init();

        // Entity methods
        Entity CreateEntity();
        void   CreateEntity(Entity entity);
        void   DestroyEntity(Entity entity);

        // Component methods
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

        // System methods
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
    };
#pragma warning(pop)
}

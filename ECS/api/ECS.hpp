#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

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
        ECS() = default;
        // Destroys the systems first: a system may hold a component-removed callback or other
        // state that refers to the component storage, which must still exist while it goes.
        ECS_API ~ECS();

        ECS(const ECS&)            = delete;
        ECS& operator=(const ECS&) = delete;
        // Movable so a fully built ECS can be returned from a helper; do not move one whose
        // systems keep a reference to it.
        ECS(ECS&&) noexcept            = default;
        ECS& operator=(ECS&&) noexcept = default;

        ECS_API void Init();

        ECS_API Entity CreateEntity();
        ECS_API void   CreateEntity(Entity entity);
        ECS_API void   DestroyEntity(Entity entity);

        ECS_API Entity GetRoot()             const;
        ECS_API void   SetRoot(Entity entity);

        // False once the entity is destroyed, and for ids never created.
        ECS_API bool     IsAlive(Entity entity)       const;
        // Changes every time the id is destroyed; pair it with the id to detect a
        // handle to an entity that was destroyed and its id reused.
        ECS_API uint32_t GetGeneration(Entity entity) const;

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

        // Called once for each T removed, by RemoveComponent<T> or DestroyEntity,
        // while the data is still readable. One callback per component type; a
        // new one replaces the old, an empty one clears it.
        template<typename T>
        void SetComponentRemovedCallback(std::function<void(Entity, T&)> callback)
        {
            m_ComponentManager->SetComponentRemovedCallback<T>(std::move(callback));
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

        // Constructs T from args; the ECS owns it from then on.
        template<typename T, typename... Args>
        std::shared_ptr<T> RegisterSystem(Args&&... args)
        {
            return m_SystemManager->RegisterSystem<T>(std::forward<Args>(args)...);
        }

        // Every registered system in registration order; the Simulation runs the play-mode
        // hooks through these.
        template<typename Fn>
        void ForEachSystem(Fn&& fn) const
        {
            m_SystemManager->ForEachSystem(std::forward<Fn>(fn));
        }

        template<typename Fn>
        void ForEachSystemReverse(Fn&& fn) const
        {
            m_SystemManager->ForEachSystemReverse(std::forward<Fn>(fn));
        }

        template<typename T>
        void SetSystemSignature(Signature signature)
        {
            m_SystemManager->SetSignature<T>(signature);

            // A system set up after entities exist must still see the ones that match.
            for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
            {
                if (m_EntityManager->IsAlive(entity))
                {
                    m_SystemManager->EntityChangedSignatureFor<T>(entity, m_EntityManager->GetSignature(entity));
                }
            }
        }

        template<typename T>
        bool HasSystem() const
        {
            return m_SystemManager->HasSystem<T>();
        }

        template<typename T>
        std::shared_ptr<T> GetSystem() const
        {
            return m_SystemManager->GetSystem<T>();
        }

    private:
        std::unique_ptr<ComponentManager> m_ComponentManager;
        std::unique_ptr<EntityManager>    m_EntityManager;
        std::unique_ptr<SystemManager>    m_SystemManager;

        Entity m_RootEntity{INVALID_ENTITY};
    };
}

#include "api/ECS.hpp"

#include <cassert>

namespace ECS
{
    ECS::~ECS()
    {
        m_SystemManager.reset();
        m_EntityManager.reset();
        m_ComponentManager.reset();
    }

    ECS::ECS(ECS&& other) noexcept            = default;
    ECS& ECS::operator=(ECS&& other) noexcept = default;

    void ECS::Init()
    {
        m_ComponentManager = std::make_unique<ComponentManager>();
        m_EntityManager    = std::make_unique<EntityManager>();
        m_SystemManager    = std::make_unique<SystemManager>();
        m_RootEntity       = INVALID_ENTITY;
        m_PlayState        = PlayState::Edit;
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

    bool ECS::StartPlay()
    {
        if (m_PlayState != PlayState::Edit)
            return false;
        m_PlayState = PlayState::Playing;
        m_SystemManager->ForEachSystem([this](System& system) { system.OnPlayStart(*this); });
        return true;
    }

    bool ECS::PausePlay()
    {
        if (m_PlayState != PlayState::Playing)
            return false;
        m_PlayState = PlayState::Paused;
        return true;
    }

    bool ECS::ResumePlay()
    {
        if (m_PlayState != PlayState::Paused)
            return false;
        m_PlayState = PlayState::Playing;
        return true;
    }

    bool ECS::StopPlay()
    {
        if (m_PlayState == PlayState::Edit)
            return false;
        m_PlayState = PlayState::Edit;
        m_SystemManager->ForEachSystemReverse([this](System& system) { system.OnPlayStop(*this); });
        return true;
    }

    PlayState ECS::GetPlayState() const
    {
        return m_PlayState;
    }

    void ECS::RunFixedUpdate(float fixedDeltaTime)
    {
        if (m_PlayState != PlayState::Playing)
            return;
        m_SystemManager->ForEachSystem([&](System& system) { system.OnFixedUpdate(*this, fixedDeltaTime); });
    }

    void ECS::RunUpdate(float deltaTime)
    {
        if (m_PlayState != PlayState::Playing)
            return;
        m_SystemManager->ForEachSystem([&](System& system) { system.OnUpdate(*this, deltaTime); });
    }
}

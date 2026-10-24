#include "api/ECS.hpp"

#include "Profiler/api/Profiler.hpp"

#include <array>
#include <cassert>
#include <string>
#include <string_view>
#include <typeinfo>
#include <unordered_map>

namespace ECS
{
    namespace
    {
        // A system's zone name: its class name without its namespaces ("TransformSystem"), looked up
        // only while the profiler records (by the type's name, which a plugin's unloaded type no
        // longer needs to outlive: the names are interned).
        const char* ZoneName(const System& system)
        {
            if (!Profiling::IsEnabled())
                return nullptr;
            static std::unordered_map<std::string, const char*> names; // the game thread's
            const char* typeName = typeid(system).name();
            auto        found    = names.find(typeName);
            if (found == names.end())
            {
                std::string_view shortName = typeName;
                if (const size_t space = shortName.rfind(' '); space != std::string_view::npos)
                    shortName.remove_prefix(space + 1); // "class " or "struct "
                if (const size_t scope = shortName.rfind("::"); scope != std::string_view::npos)
                    shortName.remove_prefix(scope + 2);
                found = names.emplace(typeName, Profiling::InternName(shortName)).first;
            }
            return found->second;
        }

        constexpr std::array<const char*, SYSTEM_PHASE_COUNT> PHASE_ZONE_NAMES = {
            "Phase: Input", "Phase: Simulation", "Phase: Animation", "Phase: Transform", "Phase: Late", "Phase: Sync",
        };
    }

    class ECS::DispatchScope
    {
    public:
        explicit DispatchScope(ECS& ecs)
            : m_Ecs(ecs)
        {
            ++m_Ecs.m_DispatchDepth;
        }

        ~DispatchScope() { --m_Ecs.m_DispatchDepth; }

        DispatchScope(const DispatchScope&)            = delete;
        DispatchScope& operator=(const DispatchScope&) = delete;

    private:
        ECS& m_Ecs;
    };

    ECS::~ECS()
    {
        Release();
    }

    ECS::ECS(ECS&& other) noexcept = default;

    ECS& ECS::operator=(ECS&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            m_ComponentManager = std::move(other.m_ComponentManager);
            m_EntityManager    = std::move(other.m_EntityManager);
            m_SystemManager    = std::move(other.m_SystemManager);
            m_Services         = std::move(other.m_Services);
            m_RootEntity       = other.m_RootEntity;
            m_DispatchDepth    = 0;
            other.m_RootEntity = INVALID_ENTITY;
        }
        return *this;
    }

    void ECS::Release()
    {
        if (m_SystemManager)
        {
            m_SystemManager->ForEachSystemReverse([this](System& system) { system.OnUnregister(*this); });
        }
        m_SystemManager.reset();
        m_EntityManager.reset();
        m_ComponentManager.reset();
    }

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

    size_t ECS::GetEntityCount() const
    {
        return m_EntityManager->GetEntityCount();
    }

    void ECS::NotifyPlayStart()
    {
        DispatchScope scope(*this);
        m_SystemManager->ForEachSystem([this](System& system) { system.OnPlayStart(*this); });
    }

    void ECS::NotifyPlayPause()
    {
        DispatchScope scope(*this);
        m_SystemManager->ForEachSystem([this](System& system) { system.OnPlayPause(*this); });
    }

    void ECS::NotifyPlayResume()
    {
        DispatchScope scope(*this);
        m_SystemManager->ForEachSystem([this](System& system) { system.OnPlayResume(*this); });
    }

    void ECS::NotifyPlayStop()
    {
        DispatchScope scope(*this);
        m_SystemManager->ForEachSystemReverse([this](System& system) { system.OnPlayStop(*this); });
    }

    void ECS::RunFixedUpdate(float fixedDeltaTime)
    {
        DispatchScope scope(*this);
        HH_PROFILE_ZONE("FixedStep");
        {
            HH_PROFILE_ZONE("OnFixedUpdate");
            m_SystemManager->ForEachSystem([&](System& system)
            {
                const Profiling::ScopedZone zone(ZoneName(system));
                system.OnFixedUpdate(*this, fixedDeltaTime);
            });
        }
        HH_PROFILE_ZONE("OnPostFixedUpdate");
        m_SystemManager->ForEachSystem([&](System& system)
        {
            const Profiling::ScopedZone zone(ZoneName(system));
            system.OnPostFixedUpdate(*this, fixedDeltaTime);
        });
    }

    void ECS::RunUpdate(float deltaTime)
    {
        DispatchScope scope(*this);
        HH_PROFILE_ZONE("OnUpdate");
        m_SystemManager->ForEachSystem([&](System& system)
        {
            const Profiling::ScopedZone zone(ZoneName(system));
            system.OnUpdate(*this, deltaTime);
        });
    }

    void ECS::RunPhase(SystemPhase phase, const FrameContext& ctx)
    {
        DispatchScope               scope(*this);
        const Profiling::ScopedZone phaseZone(PHASE_ZONE_NAMES[static_cast<size_t>(phase)]);
        if (phase == SystemPhase::Simulation && ctx.Mode == PlayMode::Playing)
        {
            for (uint32_t step = 0; step < ctx.FixedSteps; ++step)
            {
                RunFixedUpdate(ctx.FixedDeltaTime);
            }
            RunUpdate(ctx.ScaledDeltaTime);
        }
        m_SystemManager->ForEachSystemInPhase(phase, [&](System& system)
        {
            const Profiling::ScopedZone zone(ZoneName(system));
            system.OnFrame(*this, ctx);
        });
    }

    void ECS::RunPhases(SystemPhase first, SystemPhase last, const FrameContext& ctx)
    {
        assert(first <= last && "RunPhases: first comes after last.");
        for (size_t phase = static_cast<size_t>(first); phase <= static_cast<size_t>(last); ++phase)
        {
            RunPhase(static_cast<SystemPhase>(phase), ctx);
        }
    }

    bool ECS::IsDispatching() const
    {
        return m_DispatchDepth > 0;
    }
}

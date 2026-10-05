#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/HierarchySystem.hpp"
#include "HedgehogEngine/api/ECS/systems/LightSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/TransformSystem.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/SystemPhase.hpp"

#include <string>
#include <vector>

using namespace HedgehogEngine;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    using Log = std::vector<std::string>;

    // Stands in for a script: in OnUpdate it turns the watched light about z, as a script's
    // transform write does (change the component, publish TransformChangedEvent).
    class TurningSystem : public ECS::System
    {
    public:
        TurningSystem(EventBus& bus, Log& log)
            : m_Bus(bus)
            , m_Log(log)
        {
        }

        void OnUpdate(ECS::ECS& ecs, float /*dt*/) override
        {
            m_Log.push_back("update");
            auto& transform    = ecs.GetComponent<TransformComponent>(Watched);
            transform.Rotation = HM::Vector3(0.0f, 0.0f, transform.Rotation.z() + 90.0f);
            m_Bus.Publish(TransformChangedEvent{ Watched });
        }

        ECS::Entity Watched = ECS::INVALID_ENTITY;

    private:
        EventBus& m_Bus;
        Log&      m_Log;
    };

    struct World
    {
        World()
        {
            Light = Context.GetSceneManager().CreateGameObject();
            Context.GetECS().AddComponent(Light, LightComponent{});
            Context.GetEventBus().Publish(TransformChangedEvent{ Light });

            Turning          = Context.GetECS().RegisterSystem<TurningSystem>(Context.GetEventBus(), Entries);
            Turning->Watched = Light;

            Context.GetEventBus().Subscribe<LocalMatrixUpdatedEvent>([this](const LocalMatrixUpdatedEvent& event)
            {
                if (event.entity == Light)
                {
                    Entries.push_back("local");
                }
            });
            Context.GetEventBus().Subscribe<WorldMatrixUpdatedEvent>([this](const WorldMatrixUpdatedEvent& event)
            {
                if (event.entity == Light)
                {
                    Entries.push_back("world");
                }
            });

            Context.UpdateContext(1.0f, STEP); // settles the new light in Edit mode
            Entries.clear();
        }

        ECS::ECS& Ecs() { return Context.GetECS(); }

        Log                            Entries; // outlives Context, whose system and handlers write to it
        EngineContext                  Context;
        ECS::Entity                    Light = ECS::INVALID_ENTITY;
        std::shared_ptr<TurningSystem> Turning;
    };

    // The light's direction is the world matrix's first column (LightSystem::Update).
    HM::Vector3 FirstColumn(const HM::Matrix4x4& matrix)
    {
        return HM::Vector3(matrix[0][0], matrix[0][1], matrix[0][2]);
    }
}

TEST_CASE("Frame phases - Transform and Hierarchy run in Transform, Light in Late")
{
    EngineContext context;
    CHECK(context.GetTransformSystem()->GetPhase() == ECS::SystemPhase::Transform);
    CHECK(context.GetHierarchySystem()->GetPhase() == ECS::SystemPhase::Transform);
    CHECK(context.GetLightSystem()->GetPhase() == ECS::SystemPhase::Late);
}

TEST_CASE("Frame phases - one UpdateContext runs gameplay, then local, then world matrices, then lights")
{
    World world;
    REQUIRE(world.Context.Play());

    const HM::Vector3 before = world.Ecs().GetComponent<LightComponent>(world.Light).Direction;
    world.Context.UpdateContext(1.0f, STEP);

    CHECK(world.Entries == Log{ "update", "local", "world" });

    // The light already points along the turned world matrix in the same frame.
    const auto&       transform = world.Ecs().GetComponent<TransformComponent>(world.Light);
    const HM::Vector3 after     = world.Ecs().GetComponent<LightComponent>(world.Light).Direction;
    CHECK(after == FirstColumn(transform.ObjMatrix));
    CHECK_FALSE(after == before);
}

TEST_CASE("Frame phases - in Edit mode the phases still run but gameplay does not")
{
    World world;
    world.Ecs().GetComponent<TransformComponent>(world.Light).Rotation = HM::Vector3(0.0f, 0.0f, 45.0f);
    world.Context.GetEventBus().Publish(TransformChangedEvent{ world.Light });

    world.Context.UpdateContext(1.0f, STEP);

    CHECK(world.Entries == Log{ "local", "world" });
    const auto& transform = world.Ecs().GetComponent<TransformComponent>(world.Light);
    CHECK(world.Ecs().GetComponent<LightComponent>(world.Light).Direction == FirstColumn(transform.ObjMatrix));
}

TEST_CASE("Frame phases - Paused holds gameplay while transforms still apply")
{
    World world;
    REQUIRE(world.Context.Play());
    REQUIRE(world.Context.Pause());

    world.Context.UpdateContext(1.0f, STEP);
    CHECK(world.Entries.empty());

    world.Ecs().GetComponent<TransformComponent>(world.Light).Position = HM::Vector3(1.0f, 2.0f, 3.0f);
    world.Context.GetEventBus().Publish(TransformChangedEvent{ world.Light });
    world.Context.UpdateContext(1.0f, STEP);
    CHECK(world.Entries == Log{ "local", "world" });
    CHECK(world.Ecs().GetComponent<LightComponent>(world.Light).Position == HM::Vector3(1.0f, 2.0f, 3.0f));
}

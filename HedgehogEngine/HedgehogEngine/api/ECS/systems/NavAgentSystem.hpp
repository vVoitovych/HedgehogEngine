#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/System.hpp"

namespace HedgehogEngine
{
    class EventBus;

    // Walks NavAgentComponent entities (NavAgent + Transform) along the NavigationSystem's navmesh.
    // Its work is in OnFixedUpdate, so only in Play and before the physics step of the same fixed
    // step: each agent with a destination replans when it is more than REPLAN_DISTANCE off its path,
    // accelerates towards its next corner (slowing to stop at the last), and, there, stops and
    // publishes NavAgentArrivedEvent once. With a CharacterControllerComponent its velocity becomes
    // the character's walk; otherwise its local Position moves, snapped to the mesh's height, with a
    // TransformChangedEvent.
    // ponytail: the agent's local Position is taken as its world position, so an agent must be a
    // top-level entity; go through its parent's world transform when agents need parents.
    class NavAgentSystem : public ECS::System
    {
    public:
        static constexpr float REPLAN_DISTANCE = 1.0f;

        HEDGEHOG_ENGINE_API void OnRegister(ECS::ECS& ecs) override;
        [[nodiscard]] HEDGEHOG_ENGINE_API ECS::SystemPhase GetPhase() const override;
        HEDGEHOG_ENGINE_API void OnFixedUpdate(ECS::ECS& ecs, float fixedDt) override;

        // Plans a path from the agent to point and walks it; false, and NoPath, when there is none.
        HEDGEHOG_ENGINE_API bool SetDestination(ECS::ECS& ecs, ECS::Entity entity, const HM::Vector3& point);
        // Drops the destination and stands still.
        HEDGEHOG_ENGINE_API void Stop(ECS::ECS& ecs, ECS::Entity entity);
        // Along what is left of its path, 0 without one.
        [[nodiscard]] HEDGEHOG_ENGINE_API float GetRemainingDistance(const ECS::ECS& ecs, ECS::Entity entity) const;

    private:
        bool Plan(ECS::ECS& ecs, ECS::Entity entity);
        void Steer(ECS::ECS& ecs, ECS::Entity entity, float dt);

        EventBus* m_Bus = nullptr;
    };
}

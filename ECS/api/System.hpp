#pragma once

#include "Entity.hpp"

#include <vector>

namespace ECS
{
    class ECS;
    class SystemManager;

    class System
    {
        friend class SystemManager;
    public:
        virtual ~System() = default;

        const std::vector<Entity>& GetEntities() const { return m_Entities; }

        // Play-mode hooks. The ECS calls them on every registered system, in registration order
        // (OnPlayStop in reverse), and only while playing: gameplay is a component plus a system
        // that overrides them. Each does nothing by default. A system's own per-frame work,
        // which runs in every mode, stays in its own methods, called by its owner.
        virtual void OnPlayStart(ECS& /*ecs*/) {}
        virtual void OnPlayStop(ECS& /*ecs*/) {}
        // Once per fixed simulation step; zero or more times per frame.
        virtual void OnFixedUpdate(ECS& /*ecs*/, float /*fixedDeltaTime*/) {}
        // Once per played frame, after that frame's fixed steps.
        virtual void OnUpdate(ECS& /*ecs*/, float /*deltaTime*/) {}

    protected:
        std::vector<Entity> m_Entities;
    };
}

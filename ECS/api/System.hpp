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

        // Lifecycle. OnRegister runs once, right after the ECS registered the system (GetSystem
        // already finds it): subscribe to events and look up services here. OnUnregister runs once
        // when the system leaves the ECS (at teardown, last registered first, while every component
        // is still readable): undo what OnRegister did. Both do nothing by default.
        virtual void OnRegister(ECS& /*ecs*/) {}
        virtual void OnUnregister(ECS& /*ecs*/) {}

        // Play-mode events. The ECS forwards them to every registered system in registration order
        // (OnPlayStop in reverse): gameplay is a component plus a system that overrides them. Each
        // does nothing by default. The ECS keeps no play state; whoever drives play mode decides
        // when to send each event. A system's own per-frame work, which runs in every mode, stays
        // in its own methods, called by its owner.
        virtual void OnPlayStart(ECS& /*ecs*/) {}
        virtual void OnPlayPause(ECS& /*ecs*/) {}
        virtual void OnPlayResume(ECS& /*ecs*/) {}
        virtual void OnPlayStop(ECS& /*ecs*/) {}
        // Once per fixed simulation step; zero or more times per frame.
        virtual void OnFixedUpdate(ECS& /*ecs*/, float /*fixedDeltaTime*/) {}
        // Once per played frame, after that frame's fixed steps.
        virtual void OnUpdate(ECS& /*ecs*/, float /*deltaTime*/) {}

    protected:
        std::vector<Entity> m_Entities;
    };
}

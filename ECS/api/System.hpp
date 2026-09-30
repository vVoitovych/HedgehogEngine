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

        // Play-mode hooks. The engine's Simulation calls them on every system registered in
        // the ECS, in registration order (OnPlayStop in reverse), and only while playing, so
        // gameplay is a component plus a system like everything else. Each does nothing unless
        // a system overrides it; a system's own per-frame work, which runs in every mode, is
        // called by its owner as before.
        virtual void OnPlayStart(ECS& /*ecs*/) {}
        virtual void OnPlayStop(ECS& /*ecs*/) {}
        // Once per fixed step of the simulation clock; zero or more times per frame.
        virtual void OnFixedUpdate(ECS& /*ecs*/, float /*fixedDeltaTime*/) {}
        // Once per played frame, after that frame's fixed steps, with the real frame time.
        virtual void OnUpdate(ECS& /*ecs*/, float /*deltaTime*/) {}

    protected:
        std::vector<Entity> m_Entities;
    };
}

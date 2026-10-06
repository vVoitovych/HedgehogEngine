#pragma once

#include "HedgehogEngine/api/Events/EventBus.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/SystemPhase.hpp"

namespace Spinner
{
    // Spinner + Transform. Gameplay: it turns each enabled spinner in OnUpdate (Play mode only,
    // by the frame's scaled time) and publishes TransformChangedEvent, so the Transform phase that
    // follows applies the turn the same frame.
    class SpinnerSystem : public ECS::System
    {
    public:
        void OnRegister(ECS::ECS& ecs) override;
        void OnUnregister(ECS::ECS& ecs) override;
        void OnUpdate(ECS::ECS& ecs, float dt) override;

        [[nodiscard]] ECS::SystemPhase GetPhase() const override { return ECS::SystemPhase::Simulation; }

    private:
        HedgehogEngine::EventBus* m_Bus = nullptr;
    };
}

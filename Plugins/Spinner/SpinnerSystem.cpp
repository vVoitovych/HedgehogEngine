#include "SpinnerSystem.hpp"
#include "SpinnerComponent.hpp"

#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"

#include "HedgehogMath/api/Quaternion.hpp"

namespace Spinner
{
    void SpinnerSystem::OnRegister(ECS::ECS& ecs)
    {
        m_Bus = ecs.GetServices().Find<HedgehogEngine::EventBus>();
    }

    void SpinnerSystem::OnUnregister(ECS::ECS&)
    {
        m_Bus = nullptr;
    }

    void SpinnerSystem::OnUpdate(ECS::ECS& ecs, float dt)
    {
        for (const ECS::Entity entity : m_Entities)
        {
            const SpinnerComponent& spinner = ecs.GetComponent<SpinnerComponent>(entity);
            if (!spinner.Enabled || spinner.Speed == 0.0f || dt <= 0.0f || spinner.Axis.LengthSqr() <= 0.0f)
                continue;

            // About the entity's own axis: the turn is applied after its rotation.
            auto& transform = ecs.GetComponent<HedgehogEngine::TransformComponent>(entity);
            const HM::Quaternion turned =
                HM::Quaternion::FromEuler(transform.Rotation) * HM::Quaternion::FromAxisAngle(spinner.Axis, spinner.Speed * dt);
            transform.Rotation = turned.Normalize().ToEuler();
            if (m_Bus)
                m_Bus->Publish(HedgehogEngine::TransformChangedEvent{ entity });
        }
    }
}

#include "HedgehogEngine/api/ECS/systems/TransformSystem.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"

#include "HedgehogMath/api/Vector.hpp"
#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Common.hpp"

#include <algorithm>

namespace HedgehogEngine
{
    void TransformSystem::OnRegister(ECS::ECS& ecs)
    {
        m_Bus = ecs.GetServices().Find<EventBus>();
        if (m_Bus)
        {
            m_Subscription = m_Bus->Subscribe<TransformChangedEvent>([this](const TransformChangedEvent& e) { OnTransformChanged(e); });
        }
    }

    void TransformSystem::OnUnregister(ECS::ECS& /*ecs*/)
    {
        if (m_Bus)
        {
            m_Bus->Unsubscribe(m_Subscription);
        }
        m_Bus          = nullptr;
        m_Subscription = SubscriptionId::Invalid;
    }

    void TransformSystem::OnTransformChanged(const TransformChangedEvent& event)
    {
        const auto& entities = GetEntities();
        if (std::find(entities.begin(), entities.end(), event.entity) != entities.end())
            m_PendingEntities.push_back(event.entity);
    }

    void TransformSystem::Update(ECS::ECS& ecs, EventBus& bus)
    {
        for (auto const& entity : m_PendingEntities)
        {
            // Destroyed (or stripped of its transform) since the change was published, e.g. a game
            // object created and deleted in the same frame.
            if (!ecs.IsAlive(entity) || !ecs.HasComponent<TransformComponent>(entity))
                continue;
            auto& transform = ecs.GetComponent<TransformComponent>(entity);

            HM::Matrix4x4 translation = HM::Matrix4x4::GetTranslation(transform.Position);
            HM::Matrix4x4 rotationX   = HM::Matrix4x4::GetRotationX(HM::ToRadians(transform.Rotation.x()));
            HM::Matrix4x4 rotationY   = HM::Matrix4x4::GetRotationY(HM::ToRadians(transform.Rotation.y()));
            HM::Matrix4x4 rotationZ   = HM::Matrix4x4::GetRotationZ(HM::ToRadians(transform.Rotation.z()));
            HM::Matrix4x4 scale       = HM::Matrix4x4::GetScale(
                transform.Scale.x(), transform.Scale.y(), transform.Scale.z());

            transform.LocalMatrix = translation * rotationX * rotationY * rotationZ * scale;

            bus.Publish(LocalMatrixUpdatedEvent{ entity });
        }
        m_PendingEntities.clear();
    }
}

#include "HedgehogEngine/api/ECS/systems/TransformSystem.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"

#include "HedgehogMath/api/Vector.hpp"
#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Common.hpp"

#include <algorithm>

namespace HedgehogEngine
{
    void TransformSystem::Init(EventBus& bus)
    {
        bus.Subscribe<TransformChangedEvent>([this](const TransformChangedEvent& e)
        {
            OnTransformChanged(e);
        });
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
            auto& transform = ecs.GetComponent<TransformComponent>(entity);

            HM::Matrix4x4 translation = HM::Matrix4x4::GetTranslation(transform.m_Position);
            HM::Matrix4x4 rotationX   = HM::Matrix4x4::GetRotationX(HM::ToRadians(transform.m_Rotation.x()));
            HM::Matrix4x4 rotationY   = HM::Matrix4x4::GetRotationY(HM::ToRadians(transform.m_Rotation.y()));
            HM::Matrix4x4 rotationZ   = HM::Matrix4x4::GetRotationZ(HM::ToRadians(transform.m_Rotation.z()));
            HM::Matrix4x4 scale       = HM::Matrix4x4::GetScale(
                transform.m_Scale.x(), transform.m_Scale.y(), transform.m_Scale.z());

            transform.m_LocalMatrix = translation * rotationX * rotationY * rotationZ * scale;

            bus.Publish(LocalMatrixUpdatedEvent{ entity });
        }
        m_PendingEntities.clear();
    }
}

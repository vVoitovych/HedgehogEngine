#include "HedgehogEngine/api/ECS/systems/PhysicsSystem.hpp"
#include "PhysicsWorldTransform.hpp"

#include "HedgehogEngine/api/ECS/components/JointComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"

#include "HedgehogPhysics/api/PhysicsWorld.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <string>

namespace HedgehogEngine
{
    namespace
    {
        // A point in the entity's local space, in the world.
        HM::Vector3 ToWorldPoint(const WorldTransform& world, const HM::Vector3& point)
        {
            const auto column = [&world](int index)
            {
                return HM::Vector3(world.Matrix[index].x(), world.Matrix[index].y(), world.Matrix[index].z());
            };
            return column(3) + column(0) * point.x() + column(1) * point.y() + column(2) * point.z();
        }

        bool IsJointed(ECS::ECS& ecs, ECS::Entity entity)
        {
            return ecs.IsAlive(entity) && ecs.HasComponent<JointComponent>(entity) &&
                   ecs.GetComponent<JointComponent>(entity).Enabled;
        }
    }

    void PhysicsSystem::WarnJoint(ECS::Entity entity, const std::string& why)
    {
        if (m_WarnedJoints.insert(entity).second)
            LOGWARNING("[Physics] Entity " + std::to_string(entity) + "'s joint is not made: " + why + ".");
    }

    void PhysicsSystem::SyncJoints(ECS::ECS& ecs)
    {
        if (!ecs.HasSystem<JointListSystem>())
            return;
        const std::vector<ECS::Entity>& present = ecs.GetSystem<JointListSystem>()->GetEntities();

        // Joints whose entity went, was recycled or disabled, whose connection changed, whose constraint
        // went with a body, or whose bodies were made again.
        m_GoneJoints.clear();
        for (const auto& [entity, tracked] : m_Joints)
        {
            const bool gone = std::find(present.begin(), present.end(), entity) == present.end() || !IsJointed(ecs, entity) ||
                              ecs.GetGeneration(entity) != tracked.Generation ||
                              ecs.GetComponent<JointComponent>(entity).ConnectedEntity != tracked.Connected ||
                              (tracked.Constraint.IsSet() && !m_World->IsValid(tracked.Constraint)) ||
                              FindBody(entity) != tracked.BodyA ||
                              (tracked.Connected != ECS::INVALID_ENTITY && FindBody(tracked.Connected) != tracked.BodyB);
            if (gone)
                m_GoneJoints.push_back(entity);
        }
        for (const ECS::Entity entity : m_GoneJoints)
            DestroyJoint(ecs, entity);

        for (const ECS::Entity entity : present)
            if (!m_Joints.contains(entity) && IsJointed(ecs, entity))
                CreateJoint(ecs, entity);
    }

    void PhysicsSystem::CreateJoint(ECS::ECS& ecs, ECS::Entity entity)
    {
        const JointComponent& joint     = ecs.GetComponent<JointComponent>(entity);
        const ECS::Entity     connected = joint.ConnectedEntity;
        const HP::BodyHandle  bodyA     = FindBody(entity);
        if (!bodyA.IsSet())
        {
            WarnJoint(entity, "its entity has no physics body");
            return;
        }
        HP::BodyHandle bodyB;
        if (connected == entity)
        {
            WarnJoint(entity, "it is connected to its own entity");
            return;
        }
        if (connected != ECS::INVALID_ENTITY)
        {
            // A connected entity that is gone ends the joint quietly; a live one without a body waits.
            if (!ecs.IsAlive(connected))
                return;
            bodyB = FindBody(connected);
            if (!bodyB.IsSet())
            {
                WarnJoint(entity, "its connected entity " + std::to_string(connected) + " has no physics body");
                return;
            }
        }
        if (m_World->GetMotionType(bodyA) == HP::MotionType::Static &&
            (!bodyB.IsSet() || m_World->GetMotionType(bodyB) == HP::MotionType::Static))
        {
            WarnJoint(entity, "both of its sides are static");
            return;
        }

        const WorldTransform world = ComputeWorldTransform(ecs, entity);
        HP::ConstraintDesc   desc;
        desc.Type            = static_cast<HP::ConstraintType>(joint.Type);
        desc.BodyA           = bodyA;
        desc.BodyB           = bodyB;
        desc.Anchor          = ToWorldPoint(world, joint.Anchor);
        desc.Axis            = world.Rotation.Rotate(joint.Axis);
        desc.AnchorB         = bodyB.IsSet() ? ToWorldPoint(ComputeWorldTransform(ecs, connected), joint.ConnectedAnchor)
                                             : joint.ConnectedAnchor;
        desc.LimitsEnabled   = joint.LimitsEnabled;
        desc.MinLimit        = joint.MinLimit;
        desc.MaxLimit        = joint.MaxLimit;
        desc.SpringFrequency = joint.SpringFrequency;
        desc.SpringDamping   = joint.SpringDamping;
        // The world logs why it refuses one; a refused joint is not tried again until it changes.
        const HP::ConstraintHandle constraint = m_World->CreateConstraint(desc);
        m_Joints[entity] = TrackedJoint{ ecs.GetGeneration(entity), constraint, connected, bodyA, bodyB };
        ecs.GetComponent<JointComponent>(entity).Constraint = constraint;
    }

    void PhysicsSystem::DestroyJoint(ECS::ECS& ecs, ECS::Entity entity)
    {
        const auto found = m_Joints.find(entity);
        if (found == m_Joints.end())
            return;
        if (m_World)
            m_World->DestroyConstraint(found->second.Constraint);
        m_Joints.erase(found);
        if (ecs.IsAlive(entity) && ecs.HasComponent<JointComponent>(entity))
            ecs.GetComponent<JointComponent>(entity).Constraint = HP::ConstraintHandle{};
    }

    void PhysicsSystem::DestroyJoints(ECS::ECS& ecs)
    {
        while (!m_Joints.empty())
            DestroyJoint(ecs, m_Joints.begin()->first);
        m_WarnedJoints.clear();
    }

    void PhysicsSystem::RebuildJoint(ECS::ECS& ecs, ECS::Entity entity)
    {
        if (!m_World || !m_World->IsInitialized())
            return;
        DestroyJoint(ecs, entity);
        m_WarnedJoints.erase(entity);
        if (IsJointed(ecs, entity) && ecs.HasComponent<TransformComponent>(entity))
            CreateJoint(ecs, entity);
    }

    HP::ConstraintHandle PhysicsSystem::GetJoint(ECS::Entity entity) const
    {
        const auto found = m_Joints.find(entity);
        return found != m_Joints.end() ? found->second.Constraint : HP::ConstraintHandle{};
    }

    uint32_t PhysicsSystem::GetJointCount() const
    {
        return static_cast<uint32_t>(std::count_if(m_Joints.begin(), m_Joints.end(), [this](const auto& entry) {
            return m_World && m_World->IsValid(entry.second.Constraint);
        }));
    }
}

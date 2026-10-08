#include "HedgehogEngine/api/ECS/systems/PhysicsSystem.hpp"
#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RigidBodyComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/PhysicsEvents.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"

#include "HedgehogPhysics/api/PhysicsWorld.hpp"
#include "HedgehogSettings/api/HedgehogSettings.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include "HedgehogMath/api/Common.hpp"
#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Quaternion.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <thread>

namespace HedgehogEngine
{
    namespace
    {
        // A non-uniform scale counts as uniform within this.
        constexpr float UNIFORM_SCALE_TOLERANCE = 1e-4f;

        // An entity's transform in the world, composed from the components themselves (the
        // ObjMatrix may be a frame old): its matrix, its rotation, the product of the scales, and
        // whether an ancestor is scaled unevenly (the rotation and scale are then approximate).
        struct WorldTransform
        {
            HM::Matrix4x4  Matrix   = HM::Matrix4x4::GetIdentity();
            HM::Quaternion Rotation = HM::Quaternion::Identity();
            HM::Vector3    Scale    = HM::Vector3(1.0f, 1.0f, 1.0f);
            bool           UnevenAncestor = false;
        };

        // As TransformSystem builds it: T * Rx * Ry * Rz * S.
        HM::Matrix4x4 MakeLocalMatrix(const TransformComponent& transform)
        {
            return HM::Matrix4x4::GetTranslation(transform.Position) *
                   HM::Matrix4x4::GetRotationX(HM::ToRadians(transform.Rotation.x())) *
                   HM::Matrix4x4::GetRotationY(HM::ToRadians(transform.Rotation.y())) *
                   HM::Matrix4x4::GetRotationZ(HM::ToRadians(transform.Rotation.z())) *
                   HM::Matrix4x4::GetScale(transform.Scale.x(), transform.Scale.y(), transform.Scale.z());
        }

        bool IsUniform(const HM::Vector3& scale)
        {
            const float largest = std::max({ std::abs(scale.x()), std::abs(scale.y()), std::abs(scale.z()) });
            const float least   = std::min({ std::abs(scale.x()), std::abs(scale.y()), std::abs(scale.z()) });
            return largest - least <= UNIFORM_SCALE_TOLERANCE * largest;
        }

        WorldTransform ComputeWorldTransform(ECS::ECS& ecs, ECS::Entity entity)
        {
            // The entity and its ancestors, the entity first; the root is its own parent.
            ECS::Entity chain[64];
            size_t      count = 0;
            for (ECS::Entity current = entity; count < std::size(chain);)
            {
                chain[count++] = current;
                if (!ecs.HasComponent<ECS::HierarchyComponent>(current))
                    break;
                const ECS::Entity parent = ecs.GetComponent<ECS::HierarchyComponent>(current).Parent;
                if (parent == current || parent == ECS::INVALID_ENTITY || !ecs.IsAlive(parent) ||
                    !ecs.HasComponent<TransformComponent>(parent))
                    break;
                current = parent;
            }

            WorldTransform world;
            for (size_t index = count; index-- > 0;)
            {
                const TransformComponent& transform = ecs.GetComponent<TransformComponent>(chain[index]);
                world.Matrix   = world.Matrix * MakeLocalMatrix(transform);
                world.Rotation = world.Rotation * HM::Quaternion::FromEuler(transform.Rotation);
                world.Scale    = HM::Vector3(world.Scale.x() * transform.Scale.x(), world.Scale.y() * transform.Scale.y(),
                                             world.Scale.z() * transform.Scale.z());
                if (index > 0 && !IsUniform(transform.Scale))
                    world.UnevenAncestor = true;
            }
            return world;
        }

        HP::ShapeDesc MakeShape(const ColliderComponent& collider, const HM::Vector3& scale)
        {
            HP::ShapeDesc shape;
            shape.Center = collider.Center;
            shape.Scale  = scale;
            shape.Radius = collider.Radius;
            switch (collider.Shape)
            {
            case ColliderShape::Box:
                shape.Type        = HP::ShapeType::Box;
                shape.HalfExtents = HM::Vector3(collider.Size.x() * 0.5f, collider.Size.y() * 0.5f, collider.Size.z() * 0.5f);
                break;
            case ColliderShape::Sphere:
                shape.Type = HP::ShapeType::Sphere;
                break;
            case ColliderShape::Capsule:
                // Height is end to end; Jolt's half height is the cylinder's.
                shape.Type       = HP::ShapeType::Capsule;
                shape.HalfHeight = std::max(0.0f, collider.Height * 0.5f - collider.Radius);
                break;
            }
            return shape;
        }

        HP::MotionType ToMotion(const RigidBodyComponent* body)
        {
            if (!body)
                return HP::MotionType::Static;
            switch (body->BodyType)
            {
            case RigidBodyType::Kinematic: return HP::MotionType::Kinematic;
            case RigidBodyType::Dynamic:   return HP::MotionType::Dynamic;
            default:                       return HP::MotionType::Static;
            }
        }

        HP::BodyPose ToBodyPose(const WorldTransform& world)
        {
            return { HM::Vector3(world.Matrix[3].x(), world.Matrix[3].y(), world.Matrix[3].z()), world.Rotation };
        }

        // -1 workers: the hardware's threads minus one (at least 0).
        int32_t ResolveWorkerThreads(int32_t configured)
        {
            if (configured >= 0)
                return configured;
            const int32_t hardware = static_cast<int32_t>(std::thread::hardware_concurrency());
            return std::max(0, hardware - 1);
        }
    }

    void PhysicsSystem::OnRegister(ECS::ECS& ecs)
    {
        m_World = ecs.GetServices().Find<HP::PhysicsWorld>();
        m_Bus   = ecs.GetServices().Find<EventBus>();
    }

    void PhysicsSystem::OnUnregister(ECS::ECS& ecs)
    {
        DestroyBodies(ecs);
        m_World = nullptr;
        m_Bus   = nullptr;
    }

    void PhysicsSystem::OnPlayStart(ECS::ECS& ecs)
    {
        m_WarnedNoCollider.clear();
        m_WarnedScale.clear();
        WarnRigidBodiesWithoutCollider(ecs);
        ApplySettings(ecs);
        if (GetEntities().empty() || !StartWorld(ecs))
            return;
        SyncBodies(ecs);
    }

    void PhysicsSystem::OnPostFixedUpdate(ECS::ECS& ecs, float fixedDeltaTime)
    {
        WarnRigidBodiesWithoutCollider(ecs);
        // A collider added during Play starts the world then.
        if (GetEntities().empty() && m_Bodies.empty())
            return;
        if (!StartWorld(ecs))
            return;
        SyncBodies(ecs);
        FollowTransforms(ecs, fixedDeltaTime);
        if (m_World->Step(fixedDeltaTime))
            WriteBack(ecs);
        PublishContacts(ecs);
    }

    void PhysicsSystem::OnPlayStop(ECS::ECS& ecs)
    {
        DestroyBodies(ecs);
        m_WarnedNoCollider.clear();
        m_WarnedScale.clear();
    }

    uint32_t PhysicsSystem::GetBodyCount() const
    {
        return static_cast<uint32_t>(std::count_if(m_Bodies.begin(), m_Bodies.end(),
                                                   [](const auto& entry) { return entry.second.Body.IsSet(); }));
    }

    bool PhysicsSystem::IsWorldStarted() const { return m_World && m_World->IsInitialized(); }

    HP::BodyHandle PhysicsSystem::GetBody(ECS::Entity entity) const
    {
        const auto found = m_Bodies.find(entity);
        return found != m_Bodies.end() ? found->second.Body : HP::BodyHandle{};
    }

    bool PhysicsSystem::StartWorld(ECS::ECS& ecs)
    {
        if (!m_World)
            return false;
        if (m_World->IsInitialized())
            return true;

        HP::PhysicsWorldDesc desc;
        if (const auto* settings = ecs.GetServices().Find<HedgehogSettings::Settings>())
        {
            const HedgehogSettings::PhysicsSettings& physics = settings->GetPhysicsSettings();
            desc.Gravity       = HM::Vector3(physics.Gravity[0], physics.Gravity[1], physics.Gravity[2]);
            desc.Collisions    = physics.CollisionMasks;
            desc.WorkerThreads = ResolveWorkerThreads(physics.WorkerThreads);
        }
        return m_World->Init(desc);
    }

    void PhysicsSystem::ApplySettings(ECS::ECS& ecs)
    {
        // A world started by an earlier Play takes the settings as they are now: gameplay may have
        // changed its gravity, and the editor its matrix. Its worker threads stay as they started.
        if (!m_World || !m_World->IsInitialized())
            return;
        if (const auto* settings = ecs.GetServices().Find<HedgehogSettings::Settings>())
        {
            const HedgehogSettings::PhysicsSettings& physics = settings->GetPhysicsSettings();
            m_World->SetGravity(HM::Vector3(physics.Gravity[0], physics.Gravity[1], physics.Gravity[2]));
            m_World->SetCollisionMatrix(physics.CollisionMasks);
        }
    }

    void PhysicsSystem::SyncBodies(ECS::ECS& ecs)
    {
        m_Present.clear();
        m_Present.insert(GetEntities().begin(), GetEntities().end());

        // Entities that died, were recycled or lost their collider or transform.
        m_Gone.clear();
        for (const auto& [entity, tracked] : m_Bodies)
        {
            if (!m_Present.contains(entity) || !ecs.IsAlive(entity) || ecs.GetGeneration(entity) != tracked.Generation)
                m_Gone.push_back(entity);
        }
        for (const ECS::Entity entity : m_Gone)
        {
            const TrackedBody& tracked = m_Bodies[entity];
            if (tracked.IsTrigger)
                m_GoneTriggers.insert(entity);
            m_World->DestroyBody(tracked.Body);
            m_Bodies.erase(entity);
            if (ecs.IsAlive(entity) && ecs.HasComponent<RigidBodyComponent>(entity))
                ecs.GetComponent<RigidBodyComponent>(entity).Body = HP::BodyHandle{};
        }

        for (const ECS::Entity entity : GetEntities())
        {
            if (!m_Bodies.contains(entity))
                CreateBody(ecs, entity);
        }
    }

    void PhysicsSystem::CreateBody(ECS::ECS& ecs, ECS::Entity entity)
    {
        const ColliderComponent&  collider = ecs.GetComponent<ColliderComponent>(entity);
        RigidBodyComponent*       rigid    = ecs.HasComponent<RigidBodyComponent>(entity) ? &ecs.GetComponent<RigidBodyComponent>(entity) : nullptr;
        const WorldTransform      world    = ComputeWorldTransform(ecs, entity);
        if (world.UnevenAncestor && m_WarnedScale.insert(entity).second)
            LOGWARNING("[Physics] Entity " + std::to_string(entity) +
                       " has an unevenly scaled parent; its body's rotation and size are approximate.");

        HP::BodyDesc desc;
        desc.Shape         = MakeShape(collider, world.Scale);
        desc.Motion        = ToMotion(rigid);
        desc.Pose          = ToBodyPose(world);
        desc.Layer         = collider.Layer < 0 ? HP::PHYSICS_LAYER_COUNT : static_cast<uint32_t>(collider.Layer);
        desc.IsSensor      = collider.IsTrigger;
        desc.Friction      = collider.Friction;
        desc.Restitution   = collider.Restitution;
        desc.UserData      = static_cast<uint64_t>(entity);
        if (rigid)
        {
            desc.Mass           = rigid->Mass;
            desc.LinearDamping  = rigid->LinearDamping;
            desc.AngularDamping = rigid->AngularDamping;
            desc.GravityFactor  = rigid->GravityScale;
        }

        // A refused desc is logged once by the world and kept as a failed body, not retried.
        const TransformComponent& transform = ecs.GetComponent<TransformComponent>(entity);
        TrackedBody               tracked;
        tracked.Generation  = ecs.GetGeneration(entity);
        tracked.Body        = m_World->CreateBody(desc);
        tracked.IsTrigger   = collider.IsTrigger;
        tracked.IsKinematic = desc.Motion == HP::MotionType::Kinematic;
        tracked.Position    = transform.Position;
        tracked.Rotation    = transform.Rotation;
        m_Bodies[entity]    = tracked;
        if (rigid)
        {
            rigid->Body           = tracked.Body;
            rigid->BodyGeneration = tracked.Generation;
        }
    }

    void PhysicsSystem::DestroyBodies(ECS::ECS& ecs)
    {
        for (const auto& [entity, tracked] : m_Bodies)
        {
            if (m_World)
                m_World->DestroyBody(tracked.Body);
            if (ecs.IsAlive(entity) && ecs.GetGeneration(entity) == tracked.Generation &&
                ecs.HasComponent<RigidBodyComponent>(entity))
                ecs.GetComponent<RigidBodyComponent>(entity).Body = HP::BodyHandle{};
        }
        const bool destroyed = !m_Bodies.empty();
        m_Bodies.clear();
        if (destroyed && m_World && m_World->IsInitialized())
        {
            // One empty step reports the destroyed bodies' Exits, which belong to the Play that
            // ended: drop them, so the next Play starts with no events.
            m_World->Step(1.0f / 60.0f);
            m_Contacts.clear();
            m_World->DrainContactEvents(m_Contacts);
            m_Contacts.clear();
        }
        m_GoneTriggers.clear();
    }

    void PhysicsSystem::WarnRigidBodiesWithoutCollider(ECS::ECS& ecs)
    {
        const auto rigidBodies = ecs.GetSystem<RigidBodyListSystem>();
        if (!rigidBodies)
            return;
        for (const ECS::Entity entity : rigidBodies->GetEntities())
        {
            if (!ecs.HasComponent<ColliderComponent>(entity) && m_WarnedNoCollider.insert(entity).second)
                LOGWARNING("[Physics] Entity " + std::to_string(entity) +
                           " has a rigid body but no collider; it is not simulated.");
        }
    }

    void PhysicsSystem::WriteBack(ECS::ECS& ecs)
    {
        m_World->GetActiveBodies(m_Active);
        for (const HP::BodyHandle body : m_Active)
        {
            const ECS::Entity entity = static_cast<ECS::Entity>(m_World->GetUserData(body));
            const auto        found  = m_Bodies.find(entity);
            if (found == m_Bodies.end() || !(found->second.Body == body) ||
                m_World->GetMotionType(body) != HP::MotionType::Dynamic)
                continue;

            // World to local, through the parent's world transform.
            const HP::BodyPose  pose   = m_World->GetPose(body);
            const ECS::Entity   parent = ecs.HasComponent<ECS::HierarchyComponent>(entity)
                                             ? ecs.GetComponent<ECS::HierarchyComponent>(entity).Parent
                                             : ECS::INVALID_ENTITY;
            WorldTransform parentWorld;
            if (parent != entity && parent != ECS::INVALID_ENTITY && ecs.IsAlive(parent) &&
                ecs.HasComponent<TransformComponent>(parent))
                parentWorld = ComputeWorldTransform(ecs, parent);

            const HM::Vector4 local = parentWorld.Matrix.Inverse() *
                                      HM::Vector4(pose.Position.x(), pose.Position.y(), pose.Position.z(), 1.0f);
            TransformComponent& transform = ecs.GetComponent<TransformComponent>(entity);
            transform.Position = HM::Vector3(local.x(), local.y(), local.z());
            transform.Rotation = (parentWorld.Rotation.Inverse() * pose.Rotation).ToEuler();
            found->second.Position = transform.Position;
            found->second.Rotation = transform.Rotation;
            if (m_Bus)
                m_Bus->Publish(TransformChangedEvent{ entity });
        }
    }

    void PhysicsSystem::FollowTransforms(ECS::ECS& ecs, float fixedDeltaTime)
    {
        for (auto& [entity, tracked] : m_Bodies)
        {
            if (!tracked.Body.IsSet())
                continue;
            const TransformComponent& transform = ecs.GetComponent<TransformComponent>(entity);
            if (tracked.IsKinematic)
            {
                // A kinematic body follows its transform every step, pushing what is in its way.
                m_World->MoveKinematic(tracked.Body, ToBodyPose(ComputeWorldTransform(ecs, entity)), fixedDeltaTime);
                continue;
            }
            if (transform.Position == tracked.Position && transform.Rotation == tracked.Rotation)
                continue;
            // Gameplay moved it since the body last wrote it: a teleport.
            m_World->SetPose(tracked.Body, ToBodyPose(ComputeWorldTransform(ecs, entity)));
            tracked.Position = transform.Position;
            tracked.Rotation = transform.Rotation;
        }
    }

    void PhysicsSystem::PublishContacts(ECS::ECS& ecs)
    {
        m_Contacts.clear();
        m_World->DrainContactEvents(m_Contacts);
        const auto isTrigger = [&](ECS::Entity entity) {
            const auto found = m_Bodies.find(entity);
            return found != m_Bodies.end() ? found->second.IsTrigger : m_GoneTriggers.contains(entity);
        };
        for (const HP::ContactEvent& contact : m_Contacts)
        {
            const ECS::Entity a     = static_cast<ECS::Entity>(contact.UserDataA);
            const ECS::Entity b     = static_cast<ECS::Entity>(contact.UserDataB);
            const bool        enter = contact.Type == HP::ContactEventType::Enter;
            // A pair whose entity died before its Enter is published never began, for gameplay.
            if (enter && (!ecs.IsAlive(a) || !ecs.IsAlive(b)))
                continue;
            if (!m_Bus)
                continue;
            if (contact.IsSensor)
            {
                const bool        aIsTrigger = isTrigger(a);
                const ECS::Entity trigger    = aIsTrigger ? a : b;
                const ECS::Entity other      = aIsTrigger ? b : a;
                if (enter)
                    m_Bus->Publish(TriggerEnterEvent{ trigger, other });
                else
                    m_Bus->Publish(TriggerExitEvent{ trigger, other });
            }
            else if (enter)
                m_Bus->Publish(CollisionEnterEvent{ a, b, contact.Point, contact.Normal });
            else
                m_Bus->Publish(CollisionExitEvent{ a, b });
        }
        m_GoneTriggers.clear();
    }

    HP::BodyHandle PhysicsSystem::FindBody(ECS::Entity entity) const
    {
        if (!m_World || !m_World->IsInitialized())
            return {};
        return GetBody(entity);
    }

    HM::Vector3 PhysicsSystem::GetLinearVelocity(ECS::Entity entity) const
    {
        const HP::BodyHandle body = FindBody(entity);
        return body.IsSet() ? m_World->GetLinearVelocity(body) : HM::Vector3(0.0f, 0.0f, 0.0f);
    }

    void PhysicsSystem::SetLinearVelocity(ECS::Entity entity, const HM::Vector3& velocity)
    {
        if (const HP::BodyHandle body = FindBody(entity); body.IsSet())
            m_World->SetLinearVelocity(body, velocity);
    }

    HM::Vector3 PhysicsSystem::GetAngularVelocity(ECS::Entity entity) const
    {
        const HP::BodyHandle body = FindBody(entity);
        return body.IsSet() ? m_World->GetAngularVelocity(body) : HM::Vector3(0.0f, 0.0f, 0.0f);
    }

    void PhysicsSystem::SetAngularVelocity(ECS::Entity entity, const HM::Vector3& velocity)
    {
        if (const HP::BodyHandle body = FindBody(entity); body.IsSet())
            m_World->SetAngularVelocity(body, velocity);
    }

    void PhysicsSystem::AddForce(ECS::Entity entity, const HM::Vector3& force)
    {
        if (const HP::BodyHandle body = FindBody(entity); body.IsSet())
            m_World->AddForce(body, force);
    }

    void PhysicsSystem::AddImpulse(ECS::Entity entity, const HM::Vector3& impulse)
    {
        if (const HP::BodyHandle body = FindBody(entity); body.IsSet())
            m_World->AddImpulse(body, impulse);
    }

    void PhysicsSystem::AddTorque(ECS::Entity entity, const HM::Vector3& torque)
    {
        if (const HP::BodyHandle body = FindBody(entity); body.IsSet())
            m_World->AddTorque(body, torque);
    }

    void PhysicsSystem::AddAngularImpulse(ECS::Entity entity, const HM::Vector3& impulse)
    {
        if (const HP::BodyHandle body = FindBody(entity); body.IsSet())
            m_World->AddAngularImpulse(body, impulse);
    }

    std::optional<PhysicsRayHit> PhysicsSystem::Raycast(const HM::Vector3& origin, const HM::Vector3& direction,
                                                        float maxDistance, uint16_t layerMask) const
    {
        if (!m_World || !m_World->IsInitialized())
            return std::nullopt;
        const std::optional<HP::RayHit> hit = m_World->CastRay(origin, direction, maxDistance, layerMask);
        if (!hit)
            return std::nullopt;
        return PhysicsRayHit{ static_cast<ECS::Entity>(hit->UserData), hit->Point, hit->Normal, hit->Distance };
    }

    void PhysicsSystem::RebuildBody(ECS::ECS& ecs, ECS::Entity entity)
    {
        const HP::BodyHandle body = FindBody(entity);
        if (!body.IsSet() || !ecs.IsAlive(entity) || !ecs.HasComponent<ColliderComponent>(entity))
            return;
        const HM::Vector3 linear  = m_World->GetLinearVelocity(body);
        const HM::Vector3 angular = m_World->GetAngularVelocity(body);
        m_World->DestroyBody(body);
        m_Bodies.erase(entity);
        CreateBody(ecs, entity);
        if (const HP::BodyHandle rebuilt = GetBody(entity); rebuilt.IsSet())
        {
            m_World->SetLinearVelocity(rebuilt, linear);
            m_World->SetAngularVelocity(rebuilt, angular);
        }
    }

    HM::Vector3 PhysicsSystem::GetGravity() const
    {
        return m_World ? m_World->GetGravity() : HM::Vector3(0.0f, 0.0f, 0.0f);
    }

    void PhysicsSystem::SetGravity(const HM::Vector3& gravity)
    {
        if (m_World && m_World->IsInitialized())
            m_World->SetGravity(gravity);
    }
}

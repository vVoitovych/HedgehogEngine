#include "HedgehogEngine/api/ECS/systems/PhysicsSystem.hpp"
#include "PhysicsWorldTransform.hpp"

#include "HedgehogEngine/api/ECS/components/CharacterControllerComponent.hpp"
#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"

#include "HedgehogPhysics/api/PhysicsWorld.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <string>

namespace HedgehogEngine
{
    namespace
    {
        HM::Vector3 TranslationOf(const WorldTransform& world)
        {
            return HM::Vector3(world.Matrix[3].x(), world.Matrix[3].y(), world.Matrix[3].z());
        }

        // The parent's world transform (the identity for a top-level entity).
        WorldTransform ParentWorld(ECS::ECS& ecs, ECS::Entity entity)
        {
            const ECS::Entity parent = ecs.HasComponent<ECS::HierarchyComponent>(entity)
                                           ? ecs.GetComponent<ECS::HierarchyComponent>(entity).Parent
                                           : ECS::INVALID_ENTITY;
            if (parent != entity && parent != ECS::INVALID_ENTITY && ecs.IsAlive(parent) &&
                ecs.HasComponent<TransformComponent>(parent))
                return ComputeWorldTransform(ecs, parent);
            return {};
        }
    }

    bool PhysicsSystem::HasCharacters() const
    {
        const auto characters = m_Ecs ? m_Ecs->GetSystem<CharacterControllerListSystem>() : nullptr;
        return characters && !characters->GetEntities().empty();
    }

    bool PhysicsSystem::IsCharacterEntity(ECS::ECS& ecs, ECS::Entity entity)
    {
        if (!ecs.HasComponent<CharacterControllerComponent>(entity))
            return false;
        if (m_WarnedColliderCharacter.insert(entity).second)
            LOGWARNING("[Physics] Entity " + std::to_string(entity) +
                       " has a character controller and a collider; the collider is ignored.");
        return true;
    }

    void PhysicsSystem::SyncCharacters(ECS::ECS& ecs)
    {
        const auto characters = ecs.GetSystem<CharacterControllerListSystem>();
        if (!characters)
            return;
        const std::vector<ECS::Entity>& present = characters->GetEntities();

        // Entities that died, were recycled or lost their controller or transform.
        m_GoneCharacters.clear();
        for (const auto& [entity, tracked] : m_Characters)
            if (std::find(present.begin(), present.end(), entity) == present.end() || !ecs.IsAlive(entity) ||
                ecs.GetGeneration(entity) != tracked.Generation)
                m_GoneCharacters.push_back(entity);
        for (const ECS::Entity entity : m_GoneCharacters)
        {
            m_World->DestroyCharacter(m_Characters[entity].Character);
            m_Characters.erase(entity);
            if (ecs.IsAlive(entity) && ecs.HasComponent<CharacterControllerComponent>(entity))
                ecs.GetComponent<CharacterControllerComponent>(entity).Character = HP::CharacterHandle{};
        }

        for (const ECS::Entity entity : present)
            if (!m_Characters.contains(entity))
                CreateCharacter(ecs, entity);
    }

    void PhysicsSystem::CreateCharacter(ECS::ECS& ecs, ECS::Entity entity)
    {
        CharacterControllerComponent& controller = ecs.GetComponent<CharacterControllerComponent>(entity);
        const TransformComponent&     transform  = ecs.GetComponent<TransformComponent>(entity);

        HP::CharacterDesc desc;
        desc.Radius          = controller.Radius;
        desc.HalfHeight      = std::max(0.0f, controller.Height * 0.5f - controller.Radius);
        desc.Center          = controller.Center;
        desc.Pose.Position   = TranslationOf(ComputeWorldTransform(ecs, entity)); // upright whatever its rotation
        desc.Layer           = controller.Layer < 0 ? HP::PHYSICS_LAYER_COUNT : static_cast<uint32_t>(controller.Layer);
        desc.Mass            = controller.Mass;
        desc.MaxStrength     = controller.PushStrength;
        desc.MaxSlopeDegrees = controller.MaxSlopeAngle;
        desc.StepHeight      = controller.StepHeight;
        desc.UserData        = static_cast<uint64_t>(entity);

        // A refused desc is logged once by the world and kept as a failed character, not retried.
        TrackedCharacter tracked;
        tracked.Generation    = ecs.GetGeneration(entity);
        tracked.Character     = m_World->CreateCharacter(desc);
        tracked.Position      = transform.Position;
        m_Characters[entity]  = tracked;
        controller.Character  = tracked.Character;
        controller.Generation = tracked.Generation;
        controller.State      = m_World->GetCharacterState(tracked.Character);
    }

    void PhysicsSystem::FollowCharacterTransforms(ECS::ECS& ecs)
    {
        for (auto& [entity, tracked] : m_Characters)
        {
            if (!tracked.Character.IsSet())
                continue;
            const TransformComponent& transform = ecs.GetComponent<TransformComponent>(entity);
            if (transform.Position == tracked.Position)
                continue;
            // Gameplay moved it since it last wrote its position: a teleport.
            HP::BodyPose pose;
            pose.Position = TranslationOf(ComputeWorldTransform(ecs, entity));
            m_World->SetCharacterPose(tracked.Character, pose);
            tracked.Position = transform.Position;
        }
    }

    void PhysicsSystem::MoveCharacters(ECS::ECS& ecs, float fixedDeltaTime)
    {
        const HM::Vector3 gravity = m_World->GetGravity();
        for (auto& [entity, tracked] : m_Characters)
        {
            if (!tracked.Character.IsSet())
                continue;
            CharacterControllerComponent& controller = ecs.GetComponent<CharacterControllerComponent>(entity);
            // The walk is horizontal; a jump is the +Z part, which the world takes only on the ground.
            const HM::Vector3 velocity(controller.DesiredVelocity.x(), controller.DesiredVelocity.y(),
                                       std::max(0.0f, controller.PendingJump));
            controller.PendingJump = 0.0f;
            m_World->MoveCharacter(tracked.Character, velocity, gravity * controller.GravityScale, fixedDeltaTime);
            controller.State = m_World->GetCharacterState(tracked.Character);

            // World to local, through the parent's world transform.
            const HM::Vector3   position = controller.State.Position;
            const HM::Vector4   local    = ParentWorld(ecs, entity).Matrix.Inverse() *
                                           HM::Vector4(position.x(), position.y(), position.z(), 1.0f);
            TransformComponent& transform = ecs.GetComponent<TransformComponent>(entity);
            transform.Position            = HM::Vector3(local.x(), local.y(), local.z());
            tracked.Position              = transform.Position;
            if (m_Bus)
                m_Bus->Publish(TransformChangedEvent{ entity });
        }
    }

    void PhysicsSystem::DestroyCharacters(ECS::ECS& ecs)
    {
        for (const auto& [entity, tracked] : m_Characters)
        {
            if (m_World)
                m_World->DestroyCharacter(tracked.Character);
            if (ecs.IsAlive(entity) && ecs.GetGeneration(entity) == tracked.Generation &&
                ecs.HasComponent<CharacterControllerComponent>(entity))
            {
                CharacterControllerComponent& controller = ecs.GetComponent<CharacterControllerComponent>(entity);
                controller.Character       = HP::CharacterHandle{};
                controller.State           = HP::CharacterState{};
                controller.DesiredVelocity = HM::Vector3(0.0f, 0.0f, 0.0f);
                controller.PendingJump     = 0.0f;
            }
        }
        m_Characters.clear();
        m_WarnedColliderCharacter.clear();
    }

    const CharacterControllerComponent* PhysicsSystem::FindCharacterComponent(ECS::Entity entity) const
    {
        const auto found = m_Characters.find(entity);
        if (!m_Ecs || found == m_Characters.end() || !found->second.Character.IsSet() || !m_Ecs->IsAlive(entity) ||
            m_Ecs->GetGeneration(entity) != found->second.Generation ||
            !m_Ecs->HasComponent<CharacterControllerComponent>(entity))
            return nullptr;
        return &m_Ecs->GetComponent<CharacterControllerComponent>(entity);
    }

    void PhysicsSystem::SetCharacterMove(ECS::Entity entity, const HM::Vector3& velocity)
    {
        if (FindCharacterComponent(entity))
            m_Ecs->GetComponent<CharacterControllerComponent>(entity).DesiredVelocity = velocity;
    }

    void PhysicsSystem::CharacterJump(ECS::Entity entity, float speed)
    {
        if (FindCharacterComponent(entity))
            m_Ecs->GetComponent<CharacterControllerComponent>(entity).PendingJump = speed;
    }

    bool PhysicsSystem::IsCharacterGrounded(ECS::Entity entity) const
    {
        const CharacterControllerComponent* controller = FindCharacterComponent(entity);
        return controller && controller->State.Ground == HP::CharacterGroundState::OnGround;
    }

    HM::Vector3 PhysicsSystem::GetCharacterVelocity(ECS::Entity entity) const
    {
        const CharacterControllerComponent* controller = FindCharacterComponent(entity);
        return controller ? controller->State.LinearVelocity : HM::Vector3(0.0f, 0.0f, 0.0f);
    }

    HM::Vector3 PhysicsSystem::GetCharacterGroundNormal(ECS::Entity entity) const
    {
        const CharacterControllerComponent* controller = FindCharacterComponent(entity);
        return controller ? controller->State.GroundNormal : HM::Vector3(0.0f, 0.0f, 0.0f);
    }

    HM::Vector3 PhysicsSystem::GetCharacterGroundVelocity(ECS::Entity entity) const
    {
        const CharacterControllerComponent* controller = FindCharacterComponent(entity);
        return controller ? controller->State.GroundVelocity : HM::Vector3(0.0f, 0.0f, 0.0f);
    }

    void PhysicsSystem::RebuildCharacter(ECS::ECS& ecs, ECS::Entity entity)
    {
        if (!FindCharacterComponent(entity))
            return;
        m_World->DestroyCharacter(m_Characters[entity].Character);
        m_Characters.erase(entity);
        CreateCharacter(ecs, entity);
    }

    HP::CharacterHandle PhysicsSystem::GetCharacter(ECS::Entity entity) const
    {
        const auto found = m_Characters.find(entity);
        return found != m_Characters.end() ? found->second.Character : HP::CharacterHandle{};
    }

    uint32_t PhysicsSystem::GetCharacterCount() const
    {
        return static_cast<uint32_t>(std::count_if(m_Characters.begin(), m_Characters.end(),
                                                   [](const auto& entry) { return entry.second.Character.IsSet(); }));
    }
}

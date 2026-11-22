#include "HedgehogEngine/api/ECS/systems/NavAgentSystem.hpp"

#include "HedgehogEngine/api/ECS/components/CharacterControllerComponent.hpp"
#include "HedgehogEngine/api/ECS/components/NavAgentComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/NavigationSystem.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/NavigationEvents.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace HedgehogEngine
{
    namespace
    {
        // How far above and below the mesh a moved agent looks for its height.
        constexpr float GROUND_SEARCH = 2.0f;

        HM::Vector3 Flat(const HM::Vector3& v)
        {
            return HM::Vector3(v.x(), v.y(), 0.0f);
        }

        float FlatDistance(const HM::Vector3& a, const HM::Vector3& b)
        {
            return Flat(b - a).LengthSlow();
        }

        // The horizontal distance from point to the segment from a to b.
        float DistanceToSegment(const HM::Vector3& point, const HM::Vector3& a, const HM::Vector3& b)
        {
            const HM::Vector3 ab     = Flat(b - a);
            const float       length = HM::Dot(ab, ab);
            const float       t      = length > 0.0f ? std::clamp(HM::Dot(Flat(point - a), ab) / length, 0.0f, 1.0f) : 0.0f;
            return FlatDistance(point, a + ab * t);
        }

        float RemainingDistance(const NavAgentComponent& agent, const HM::Vector3& position)
        {
            if (agent.Corner >= agent.Path.size())
                return 0.0f;
            float distance = FlatDistance(position, agent.Path[agent.Corner]);
            for (size_t i = agent.Corner + 1; i < agent.Path.size(); ++i)
                distance += FlatDistance(agent.Path[i - 1], agent.Path[i]);
            return distance;
        }

        // Turns degrees towards target by at most step, the short way round.
        float TurnTowards(float degrees, float target, float step)
        {
            float delta = std::fmod(target - degrees + 540.0f, 360.0f) - 180.0f;
            delta       = std::clamp(delta, -step, step);
            return degrees + delta;
        }
    }

    void NavAgentSystem::OnRegister(ECS::ECS& ecs)
    {
        m_Bus = ecs.GetServices().Find<EventBus>();
    }

    ECS::SystemPhase NavAgentSystem::GetPhase() const
    {
        return ECS::SystemPhase::Simulation;
    }

    void NavAgentSystem::OnFixedUpdate(ECS::ECS& ecs, float fixedDt)
    {
        // Steering may publish events whose handlers change agents, so walk a copy.
        const std::vector<ECS::Entity> agents(GetEntities().begin(), GetEntities().end());
        for (const ECS::Entity entity : agents)
            if (ecs.IsAlive(entity) && ecs.HasComponent<NavAgentComponent>(entity))
                Steer(ecs, entity, fixedDt);
    }

    bool NavAgentSystem::SetDestination(ECS::ECS& ecs, ECS::Entity entity, const HM::Vector3& point)
    {
        if (!ecs.IsAlive(entity) || !ecs.HasComponent<NavAgentComponent>(entity) || !ecs.HasComponent<TransformComponent>(entity))
            return false;
        auto& agent          = ecs.GetComponent<NavAgentComponent>(entity);
        agent.Destination    = point;
        agent.HasDestination = true;
        return Plan(ecs, entity);
    }

    void NavAgentSystem::Stop(ECS::ECS& ecs, ECS::Entity entity)
    {
        if (!ecs.IsAlive(entity) || !ecs.HasComponent<NavAgentComponent>(entity))
            return;
        auto& agent          = ecs.GetComponent<NavAgentComponent>(entity);
        agent.HasDestination = false;
        agent.Path.clear();
        agent.Corner   = 0;
        agent.Velocity = HM::Vector3(0.0f, 0.0f, 0.0f);
        agent.Status   = NavAgentStatus::Idle;
        if (ecs.HasComponent<CharacterControllerComponent>(entity))
            ecs.GetComponent<CharacterControllerComponent>(entity).DesiredVelocity = HM::Vector3(0.0f, 0.0f, 0.0f);
    }

    float NavAgentSystem::GetRemainingDistance(const ECS::ECS& ecs, ECS::Entity entity) const
    {
        if (!ecs.IsAlive(entity) || !ecs.HasComponent<NavAgentComponent>(entity) || !ecs.HasComponent<TransformComponent>(entity))
            return 0.0f;
        return RemainingDistance(ecs.GetComponent<NavAgentComponent>(entity), ecs.GetComponent<TransformComponent>(entity).Position);
    }

    bool NavAgentSystem::Plan(ECS::ECS& ecs, ECS::Entity entity)
    {
        auto&                   agent      = ecs.GetComponent<NavAgentComponent>(entity);
        const auto              navigation = ecs.GetSystem<NavigationSystem>();
        const HM::Vector3&      position   = ecs.GetComponent<TransformComponent>(entity).Position;
        const HN::NavPathStatus status     = navigation ? navigation->FindPath(position, agent.Destination, agent.Path)
                                                        : HN::NavPathStatus::None;
        agent.Corner = 1;
        if (status == HN::NavPathStatus::None || agent.Path.empty())
        {
            agent.Path.clear();
            agent.Corner   = 0;
            agent.Velocity = HM::Vector3(0.0f, 0.0f, 0.0f);
            agent.Status   = NavAgentStatus::NoPath;
            if (ecs.HasComponent<CharacterControllerComponent>(entity))
                ecs.GetComponent<CharacterControllerComponent>(entity).DesiredVelocity = HM::Vector3(0.0f, 0.0f, 0.0f);
            return false;
        }
        agent.Corner = std::min<size_t>(1, agent.Path.size() - 1);
        agent.Status = NavAgentStatus::Moving;
        return true;
    }

    void NavAgentSystem::Steer(ECS::ECS& ecs, ECS::Entity entity, float dt)
    {
        auto& agent = ecs.GetComponent<NavAgentComponent>(entity);
        if (!agent.Enabled || !agent.HasDestination || agent.Status != NavAgentStatus::Moving)
            return;
        auto&             transform    = ecs.GetComponent<TransformComponent>(entity);
        const bool        hasCharacter = ecs.HasComponent<CharacterControllerComponent>(entity);
        const HM::Vector3 position     = transform.Position;

        // Pushed or teleported off its path: plan again from where it is.
        if (agent.Corner > 0 && DistanceToSegment(position, agent.Path[agent.Corner - 1], agent.Path[agent.Corner]) > REPLAN_DISTANCE &&
            !Plan(ecs, entity))
            return;

        const size_t last = agent.Path.size() - 1;
        while (agent.Corner < last && FlatDistance(position, agent.Path[agent.Corner]) <= agent.StoppingDistance)
            ++agent.Corner;
        const float remaining = RemainingDistance(agent, position);
        if (agent.Corner == last && remaining <= agent.StoppingDistance)
        {
            Stop(ecs, entity);
            agent.Status = NavAgentStatus::Arrived;
            if (m_Bus)
                m_Bus->Publish(NavAgentArrivedEvent{ entity });
            return;
        }

        // Towards the corner, no faster than it can still stop at the destination.
        const HM::Vector3 toCorner = Flat(agent.Path[agent.Corner] - position);
        const float       distance = toCorner.LengthSlow();
        const float       speed    = std::min(agent.Speed, std::sqrt(2.0f * agent.Acceleration * remaining));
        const HM::Vector3 desired  = distance > 0.0f ? toCorner * (speed / distance) : HM::Vector3(0.0f, 0.0f, 0.0f);
        const HM::Vector3 change   = desired - agent.Velocity;
        const float       maxChange = agent.Acceleration * dt;
        const float       changeLength = change.LengthSlow();
        agent.Velocity = changeLength > maxChange ? agent.Velocity + change * (maxChange / changeLength) : desired;

        if (hasCharacter)
            ecs.GetComponent<CharacterControllerComponent>(entity).DesiredVelocity = agent.Velocity;
        else
        {
            HM::Vector3 moved = position + agent.Velocity * dt;
            if (const auto navigation = ecs.GetSystem<NavigationSystem>())
                if (const auto ground = navigation->SamplePosition(moved, GROUND_SEARCH))
                    moved = HM::Vector3(moved.x(), moved.y(), ground->z());
            transform.Position = moved;
        }

        if (agent.FaceMovement && agent.Velocity.LengthSlow() > 0.01f)
        {
            // Local +Y faces the movement.
            const float target = std::atan2(agent.Velocity.y(), agent.Velocity.x()) * 180.0f / std::numbers::pi_v<float> - 90.0f;
            transform.Rotation = HM::Vector3(transform.Rotation.x(), transform.Rotation.y(),
                                             TurnTowards(transform.Rotation.z(), target, agent.AngularSpeed * dt));
        }
        if (m_Bus && (!hasCharacter || agent.FaceMovement))
            m_Bus->Publish(TransformChangedEvent{ entity });
    }
}

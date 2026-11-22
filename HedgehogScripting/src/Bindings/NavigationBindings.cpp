#include "Bindings.hpp"
#include "ScriptHandles.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/NavAgentComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/NavAgentSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/NavigationSystem.hpp"

#include "Logger/api/Logger.hpp"

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace HedgehogScripting::Bindings
{
    namespace
    {
        using HedgehogEngine::NavAgentComponent;
        using HedgehogEngine::NavAgentStatus;
        using NavAgentRef = ScriptComponentRef<NavAgentComponent>;

        constexpr float DEFAULT_SAMPLE_DISTANCE = 1.0f;

        const char* GetStatusName(NavAgentStatus status)
        {
            switch (status)
            {
            case NavAgentStatus::Moving: return "moving";
            case NavAgentStatus::Arrived: return "arrived";
            case NavAgentStatus::NoPath: return "nopath";
            case NavAgentStatus::Idle: break;
            }
            return "idle";
        }

        float RequireAtLeast(float value, float minimum, const char* what)
        {
            if (!std::isfinite(value) || value < minimum)
                throw std::runtime_error(std::string("NavAgent: ") + what + " must be a finite number of at least " +
                                         std::to_string(static_cast<int>(minimum)));
            return value;
        }

        // Agents walk only in Play mode (a script's top level also runs when the editor describes
        // it): outside it setDestination does nothing and the first call logs why.
        struct PlayGuard
        {
            HedgehogEngine::EngineContext& Context;
            bool                           Warned = false;

            bool Allows(const char* call)
            {
                if (Context.GetPlayState() != HedgehogEngine::PlayState::Edit)
                    return true;
                if (!Warned)
                    LOGWARNING("[Script] Nav agents walk only in Play mode; " + std::string(call) + " did nothing.");
                Warned = true;
                return false;
            }
        };

        const HM::Vector3& RequireFinite(const HM::Vector3& value, const char* what)
        {
            if (!std::isfinite(value.x()) || !std::isfinite(value.y()) || !std::isfinite(value.z()))
                throw std::runtime_error(std::string("Navigation: ") + what + " must be a finite Vector3");
            return value;
        }
    }

    void RegisterNavigation(sol::state& lua, HedgehogEngine::EngineContext& context)
    {
        ECS::ECS&                       ecs    = context.GetECS();
        HedgehogEngine::NavAgentSystem& agents = *context.GetNavAgentSystem();
        auto                            guard  = std::make_shared<PlayGuard>(PlayGuard{ context });
        lua.new_usertype<NavAgentRef>(
            "NavAgent", sol::no_constructor,
            // setDestination(point): plans a path there and walks it; false when there is none.
            "setDestination", [&ecs, &agents, guard](const NavAgentRef& ref, const HM::Vector3& point)
            {
                (void)Resolve(ecs, ref);
                RequireFinite(point, "a destination");
                return guard->Allows("NavAgent:setDestination()") && agents.SetDestination(ecs, ref.Entity.Id, point);
            },
            "stop", [&ecs, &agents](const NavAgentRef& ref)
            {
                (void)Resolve(ecs, ref);
                agents.Stop(ecs, ref.Entity.Id);
            },
            "hasPath", [&ecs](const NavAgentRef& ref) { return !Resolve(ecs, ref).Path.empty(); },
            "remainingDistance", sol::readonly_property([&ecs, &agents](const NavAgentRef& ref)
            {
                (void)Resolve(ecs, ref);
                return agents.GetRemainingDistance(ecs, ref.Entity.Id);
            }),
            "velocity", sol::readonly_property([&ecs](const NavAgentRef& ref) { return Resolve(ecs, ref).Velocity; }),
            "status", sol::readonly_property([&ecs](const NavAgentRef& ref) { return GetStatusName(Resolve(ecs, ref).Status); }),
            "speed", sol::property(
                [&ecs](const NavAgentRef& ref) { return Resolve(ecs, ref).Speed; },
                [&ecs](const NavAgentRef& ref, float value) { Resolve(ecs, ref).Speed = RequireAtLeast(value, 0.0f, "a speed"); }),
            "acceleration", sol::property(
                [&ecs](const NavAgentRef& ref) { return Resolve(ecs, ref).Acceleration; },
                [&ecs](const NavAgentRef& ref, float value)
                { Resolve(ecs, ref).Acceleration = RequireAtLeast(value, 0.0f, "an acceleration"); }),
            "stoppingDistance", sol::property(
                [&ecs](const NavAgentRef& ref) { return Resolve(ecs, ref).StoppingDistance; },
                [&ecs](const NavAgentRef& ref, float value)
                { Resolve(ecs, ref).StoppingDistance = RequireAtLeast(value, 0.0f, "a stopping distance"); }),
            "faceMovement", sol::property(
                [&ecs](const NavAgentRef& ref) { return Resolve(ecs, ref).FaceMovement; },
                [&ecs](const NavAgentRef& ref, bool value) { Resolve(ecs, ref).FaceMovement = value; }),
            sol::meta_function::to_string, ToText<NavAgentComponent>("NavAgent"));

        HedgehogEngine::NavigationSystem& navigation = *context.GetNavigationSystem();
        sol::table                        table      = lua.create_named_table("Navigation");

        table["hasNavMesh"] = [&navigation]() { return navigation.HasNavMesh(); };

        // findPath(from, to): { status = "complete"|"partial", corners = { Vector3, ... } }, or nil.
        table["findPath"] = [&navigation](const HM::Vector3& from, const HM::Vector3& to, sol::this_state state) -> sol::object
        {
            RequireFinite(from, "a path's start");
            RequireFinite(to, "a path's end");
            std::vector<HM::Vector3> corners;
            const HN::NavPathStatus  status = navigation.FindPath(from, to, corners);
            if (status == HN::NavPathStatus::None)
                return sol::lua_nil;
            sol::state_view view(state);
            sol::table      list = view.create_table(static_cast<int>(corners.size()), 0);
            for (size_t i = 0; i < corners.size(); ++i)
                list[i + 1] = corners[i];
            sol::table result = view.create_table();
            result["status"]  = status == HN::NavPathStatus::Complete ? "complete" : "partial";
            result["corners"] = list;
            return result;
        };

        // samplePosition(point, maxDistance?): the nearest point on the mesh within the distance, or nil.
        table["samplePosition"] = [&navigation](const HM::Vector3& point, sol::optional<float> maxDistance) -> sol::optional<HM::Vector3>
        {
            RequireFinite(point, "a point");
            const float distance = maxDistance.value_or(DEFAULT_SAMPLE_DISTANCE);
            if (!std::isfinite(distance) || distance < 0.0f)
                throw std::runtime_error("Navigation: a sample distance must be a finite number of at least 0");
            if (const auto position = navigation.SamplePosition(point, distance))
                return *position;
            return sol::nullopt;
        };

        // raycast(from, to): where a walk along the surface is blocked, { point, normal }, or nil.
        table["raycast"] = [&navigation](const HM::Vector3& from, const HM::Vector3& to, sol::this_state state) -> sol::object
        {
            RequireFinite(from, "a ray's start");
            RequireFinite(to, "a ray's end");
            const auto hit = navigation.Raycast(from, to);
            if (!hit)
                return sol::lua_nil;
            sol::table result = sol::state_view(state).create_table();
            result["point"]   = hit->Point;
            result["normal"]  = hit->Normal;
            return result;
        };
    }
}

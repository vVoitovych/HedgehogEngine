#include "Bindings.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/systems/NavigationSystem.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace HedgehogScripting::Bindings
{
    namespace
    {
        constexpr float DEFAULT_SAMPLE_DISTANCE = 1.0f;

        const HM::Vector3& RequireFinite(const HM::Vector3& value, const char* what)
        {
            if (!std::isfinite(value.x()) || !std::isfinite(value.y()) || !std::isfinite(value.z()))
                throw std::runtime_error(std::string("Navigation: ") + what + " must be a finite Vector3");
            return value;
        }
    }

    void RegisterNavigation(sol::state& lua, HedgehogEngine::EngineContext& context)
    {
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

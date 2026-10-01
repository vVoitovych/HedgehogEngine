#include "Bindings.hpp"

#include "HedgehogEngine/api/Time/FixedStepClock.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace HedgehogScripting::Bindings
{
    namespace
    {
        constexpr float MAX_TIME_SCALE = 100.0f;
    }

    void RegisterTime(sol::state& lua, HedgehogEngine::FixedStepClock& clock, const float& deltaTime,
                      const uint64_t& frame)
    {
        // An empty table whose fields are read through __index, so every read sees the current
        // value; only timeScale can be written.
        sol::table time = lua.create_named_table("Time");
        sol::table meta = lua.create_table();

        meta[sol::meta_function::index] = [&clock, &deltaTime, &frame](sol::table, const std::string& key,
                                                                       sol::this_state state) -> sol::object
        {
            if (key == "deltaTime")
                return sol::make_object(state, deltaTime);
            if (key == "fixedDeltaTime")
                return sol::make_object(state, clock.FixedDeltaTime);
            if (key == "time")
                return sol::make_object(state, clock.Time);
            if (key == "frame")
                return sol::make_object(state, frame);
            if (key == "timeScale")
                return sol::make_object(state, clock.TimeScale);
            return sol::lua_nil;
        };

        // The engine's clock scales both the fixed steps and OnUpdate's dt from the next frame on.
        meta[sol::meta_function::new_index] = [&clock](sol::table, const std::string& key, sol::object value)
        {
            if (key != "timeScale")
                throw std::runtime_error("Time." + key + " is read-only; only Time.timeScale can be set");
            if (!value.is<double>() || !std::isfinite(value.as<double>()))
                throw std::runtime_error("Time.timeScale must be a finite number");
            clock.TimeScale = std::clamp(value.as<float>(), 0.0f, MAX_TIME_SCALE);
        };

        time[sol::metatable_key] = meta;
    }
}

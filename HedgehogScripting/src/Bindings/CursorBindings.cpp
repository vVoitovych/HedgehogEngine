#include "Bindings.hpp"

#include "HedgehogEngine/HedgehogWindow/api/CursorMode.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"

#include "Logger/api/Logger.hpp"

#include <array>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace HedgehogScripting::Bindings
{
    namespace
    {
        constexpr std::array<std::pair<std::string_view, HW::CursorMode>, 4> CURSOR_MODES = { {
            { "Normal", HW::CursorMode::Normal },
            { "Hidden", HW::CursorMode::Hidden },
            { "Locked", HW::CursorMode::Locked },
            { "Confined", HW::CursorMode::Confined },
        } };

        std::optional<HW::CursorMode> FindCursorMode(std::string_view name)
        {
            for (const auto& [modeName, mode] : CURSOR_MODES)
            {
                if (modeName == name)
                {
                    return mode;
                }
            }
            return std::nullopt;
        }

        std::string_view GetCursorModeName(HW::CursorMode mode)
        {
            for (const auto& [modeName, value] : CURSOR_MODES)
            {
                if (value == mode)
                {
                    return modeName;
                }
            }
            return "Normal";
        }
    }

    void RegisterCursor(sol::state& lua, HedgehogEngine::EngineContext& context)
    {
        sol::table cursor = lua.create_named_table("Cursor");
        sol::table meta   = lua.create_table();

        // Outside Play (a script's top level also runs when the editor describes it) nothing is
        // asked, and the first change says why.
        auto warned  = std::make_shared<bool>(false);
        auto request = [&context, warned](HW::CursorMode mode, const char* call)
        {
            if (context.GetPlayState() == HedgehogEngine::PlayState::Edit)
            {
                if (!*warned)
                {
                    LOGWARNING("[Script] The cursor changes only in Play mode; ", call, " did nothing.");
                }
                *warned = true;
                return;
            }
            context.GetApplicationRequests().Cursor = mode;
        };

        meta.set_function("lock", [request]() { request(HW::CursorMode::Locked, "Cursor.lock"); });
        meta.set_function("unlock", [request]() { request(HW::CursorMode::Normal, "Cursor.unlock"); });
        meta.set_function("isLocked",
                          [&context]() { return context.GetApplicationRequests().Cursor == HW::CursorMode::Locked; });

        // The table stays empty, so every read reaches __index and every write __newindex; the
        // functions live in the metatable, read from there so no closure holds a reference into
        // the state.
        meta[sol::meta_function::index] = [&context](sol::table self, const sol::object& key,
                                                     sol::this_state state) -> sol::object
        {
            if (!key.is<std::string>())
            {
                return sol::lua_nil;
            }
            const std::string name = key.as<std::string>();
            if (name == "mode")
            {
                return sol::make_object(state, std::string(GetCursorModeName(context.GetApplicationRequests().Cursor)));
            }
            if (name == "lock" || name == "unlock" || name == "isLocked")
            {
                return self[sol::metatable_key].get<sol::table>().raw_get<sol::object>(name);
            }
            return sol::lua_nil;
        };
        meta[sol::meta_function::new_index] = [request](sol::table, const sol::object& key, const sol::object& value)
        {
            const std::string name = key.is<std::string>() ? key.as<std::string>() : "<key>";
            if (name != "mode")
            {
                throw std::runtime_error("Cursor." + name + " is read-only");
            }
            if (!value.is<std::string>())
            {
                throw std::runtime_error("Cursor.mode takes a string, not " +
                                         std::string(sol::type_name(value.lua_state(), value.get_type())));
            }
            const std::string modeName = value.as<std::string>();
            const auto        mode     = FindCursorMode(modeName);
            if (!mode)
            {
                throw std::runtime_error("Cursor.mode: no mode '" + modeName +
                                         "'; the modes are Normal, Hidden, Locked and Confined");
            }
            request(*mode, "Cursor.mode");
        };
        // getmetatable gives this, and setmetatable refuses, so no script takes the table from the others.
        meta[sol::meta_function::metatable] = "Cursor";
        cursor[sol::metatable_key]          = meta;
    }
}

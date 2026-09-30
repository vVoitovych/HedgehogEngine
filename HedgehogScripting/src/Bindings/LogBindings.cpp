#include "Bindings.hpp"

#include "Logger/api/Logger.hpp"

#include <string>

namespace HedgehogScripting::Bindings
{
    namespace
    {
        enum class Level
        {
            Info,
            Warning,
            Error
        };

        // Where the calling Lua function is: "assets://Scripts/Player.lua:12".
        std::string CallerLocation(lua_State* L)
        {
            lua_Debug frame{};
            if (lua_getstack(L, 1, &frame) == 0 || lua_getinfo(L, "Sl", &frame) == 0)
                return "?";

            std::string location = frame.source != nullptr ? frame.source : "?";
            // '@' marks a file name and '=' a literal one; neither is part of the name.
            if (!location.empty() && (location.front() == '@' || location.front() == '='))
                location.erase(0, 1);
            if (frame.currentline > 0)
                location += ":" + std::to_string(frame.currentline);
            return location;
        }

        template<Level level>
        int Write(lua_State* L)
        {
            // Convert every argument while no C++ object is alive: luaL_tolstring runs __tostring,
            // which can raise a Lua error, and that must not unwind through a std::string.
            const int count = lua_gettop(L);
            luaL_checkstack(L, count, "too many arguments to log");
            for (int i = 1; i <= count; ++i)
                luaL_tolstring(L, i, nullptr);

            // From here nothing raises a Lua error.
            std::string message = "[Script] " + CallerLocation(L) + ":";
            for (int i = count + 1; i <= 2 * count; ++i)
            {
                size_t      length = 0;
                const char* text   = lua_tolstring(L, i, &length);
                message += ' ';
                message.append(text, length);
            }

            if constexpr (level == Level::Info)
                LOGINFO(message);
            else if constexpr (level == Level::Warning)
                LOGWARNING(message);
            else
                LOGERROR(message);
            return 0;
        }
    }

    void RegisterLog(sol::state& lua)
    {
        sol::table log = lua.create_named_table("Log");
        log["info"]    = &Write<Level::Info>;
        log["warn"]    = &Write<Level::Warning>;
        log["error"]   = &Write<Level::Error>;

        // Lua's own print writes straight to stdout, past Logger and the Editor's console.
        lua["print"] = &Write<Level::Info>;
    }
}

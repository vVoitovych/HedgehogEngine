#include "Sandbox.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

namespace HedgehogScripting
{
    namespace
    {
        // Runs once, with the debug library still open, and leaves only the sandbox.
        constexpr std::string_view SANDBOX_SETUP = R"lua(
            local rawLoad, rawCollect = load, collectgarbage
            local clock, time = os.clock, os.time

            -- Text chunks only: a binary chunk (string.dump) can crash the VM.
            load = function(chunk, chunkName, _, env)
                return rawLoad(chunk, chunkName, "t", env)
            end

            collectgarbage = function(option, ...)
                if option == "count" then
                    return rawCollect("count")
                end
                error("collectgarbage is disabled in scripts (only \"count\" is allowed)", 2)
            end

            os = { clock = clock, time = time }

            dofile, loadfile, require = nil, nil, nil
            io, debug, package = nil, nil, nil
        )lua";

        // print: its arguments through tostring, joined by spaces, as one Logger info line.
        int Print(lua_State* L)
        {
            // Convert every argument while no C++ object is alive: luaL_tolstring runs
            // __tostring, which can raise a Lua error, and that must not unwind through a
            // std::string.
            const int count = lua_gettop(L);
            luaL_checkstack(L, count, "too many arguments to print");
            for (int i = 1; i <= count; ++i)
                luaL_tolstring(L, i, nullptr);

            // From here nothing raises a Lua error.
            std::string message;
            for (int i = count + 1; i <= 2 * count; ++i)
            {
                size_t      length = 0;
                const char* text   = lua_tolstring(L, i, &length);
                if (i > count + 1)
                    message += ' ';
                message.append(text, length);
            }
            LOGINFO(message);
            return 0;
        }
    }

    sol::protected_function OpenSandbox(sol::state& lua)
    {
        lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table,
                           sol::lib::coroutine, sol::lib::utf8, sol::lib::os, sol::lib::debug);

        const sol::object traceback = lua["debug"]["traceback"];
        sol::protected_function::set_default_handler(traceback);

        lua["print"] = &Print;

        const sol::protected_function_result result =
            lua.safe_script(SANDBOX_SETUP, sol::script_pass_on_error, "=sandbox setup");
        if (!result.valid())
        {
            const sol::error error = result;
            ReportScriptError(std::string("sandbox setup failed: ") + error.what());
        }
        return sol::protected_function(traceback);
    }

    std::optional<sol::protected_function> LoadScriptSource(sol::state&                    lua,
                                                            std::string_view               source,
                                                            const std::string&             chunkName,
                                                            const sol::protected_function& traceback)
    {
        try
        {
            sol::load_result loaded = lua.load(source, chunkName, sol::load_mode::text);
            if (!loaded.valid())
            {
                const sol::error error = loaded;
                ReportScriptError(error.what());
                return std::nullopt;
            }
            sol::protected_function chunk = loaded;
            chunk.set_error_handler(traceback);
            return chunk;
        }
        catch (const std::exception& e)
        {
            ReportScriptError("loading '" + chunkName + "': " + e.what());
            return std::nullopt;
        }
    }

    std::optional<sol::protected_function> LoadScriptFile(sol::state&                    lua,
                                                          const FS::FileSystemManager&   fileSystem,
                                                          const std::string&             virtualPath,
                                                          const sol::protected_function& traceback)
    {
        const std::optional<std::string> source = fileSystem.ReadTextFile(virtualPath);
        if (!source)
        {
            ReportScriptError("cannot read script '" + virtualPath + "'.");
            return std::nullopt;
        }
        return LoadScriptSource(lua, *source, "@" + virtualPath, traceback);
    }

    void ReportScriptError(const std::string& message)
    {
        LOGERROR("[Lua Error] ", message);
    }
}

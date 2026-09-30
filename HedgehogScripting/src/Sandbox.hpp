#pragma once

#include "HedgehogScripting/api/Sol.hpp"

#include "Logger/api/Logger.hpp"

#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace FS
{
    class FileSystemManager;
}

namespace HedgehogScripting
{
    // Turns lua into the scripting sandbox. Scripts get only the base, math, string, table,
    // coroutine and utf8 libraries, plus os.clock and os.time. io, the rest of os, debug,
    // package, require, dofile and loadfile are removed; load takes text chunks only and
    // collectgarbage only "count"; print writes one Logger info line, so script output reaches
    // the Editor's console. Returns debug.traceback, taken before the debug library goes, and
    // also makes it lua's default error handler, so every protected function made from lua
    // after this call reports a stack traceback.
    [[nodiscard]] sol::protected_function OpenSandbox(sol::state& lua);

    // Compiles text source (never a binary chunk). chunkName appears in error messages; by
    // Lua's convention an '@' prefix marks it as a file name. A syntax error is logged and
    // gives nullopt; nothing runs.
    [[nodiscard]] std::optional<sol::protected_function> LoadScriptSource(sol::state&                    lua,
                                                                          std::string_view               source,
                                                                          const std::string&             chunkName,
                                                                          const sol::protected_function& traceback);

    // Reads virtualPath (e.g. "assets://Scripts/Player.lua") through the engine file system and
    // compiles it as the chunk "@" + virtualPath. A missing file is logged and gives nullopt.
    [[nodiscard]] std::optional<sol::protected_function> LoadScriptFile(sol::state&                    lua,
                                                                        const FS::FileSystemManager&   fileSystem,
                                                                        const std::string&             virtualPath,
                                                                        const sol::protected_function& traceback);

    // Logs one script error as "[Lua Error] <message>".
    void ReportScriptError(const std::string& message);

    // Calls function with args. A Lua error, with its traceback when function has the sandbox's
    // error handler, is logged and gives false. No exception leaves.
    template<typename... Args>
    [[nodiscard]] bool CallProtected(const sol::protected_function& function, Args&&... args)
    {
        try
        {
            const sol::protected_function_result result = function(std::forward<Args>(args)...);
            if (result.valid())
                return true;
            const sol::error error = result;
            ReportScriptError(error.what());
        }
        catch (const std::exception& e)
        {
            ReportScriptError(e.what());
        }
        return false;
    }
}

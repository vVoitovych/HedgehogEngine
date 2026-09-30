#pragma once

#include "HedgehogScripting/api/Sol.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace FS
{
    class FileSystemManager;
}

namespace HedgehogScripting
{
    // One sandboxed Lua VM. Scripts get only the base, math, string, table,
    // coroutine and utf8 libraries, plus os.clock and os.time. Nothing that reaches
    // the file system, the process or the debug library is left. Source is read
    // through the engine's file system, never Lua's own file functions.
    //
    // Every call is protected: a failure is logged with its chunk name, line and a
    // stack traceback, kept as GetLastError(), and reported by the return value.
    // No Lua or sol2 exception leaves this class.
    class ScriptVM
    {
    public:
        explicit ScriptVM(const FS::FileSystemManager& fileSystem);

        ScriptVM(const ScriptVM&)            = delete;
        ScriptVM& operator=(const ScriptVM&) = delete;

        // Compiles text source (never a binary chunk). chunkName appears in error
        // messages; by Lua's convention an '@' prefix marks it as a file name.
        [[nodiscard]] std::optional<sol::protected_function> Load(std::string_view source,
                                                                  const std::string& chunkName);
        // Reads virtualPath (e.g. "assets://Scripts/Player.lua") and compiles it as
        // the chunk "@" + virtualPath.
        [[nodiscard]] std::optional<sol::protected_function> LoadFile(const std::string& virtualPath);

        // Load, then run the chunk once.
        [[nodiscard]] bool Run(std::string_view source, const std::string& chunkName);
        [[nodiscard]] bool RunFile(const std::string& virtualPath);

        // Calls a loaded function with the traceback handler installed.
        [[nodiscard]] bool Call(const sol::protected_function& function);

        sol::state&        GetState();
        const std::string& GetLastError() const;

    private:
        void OpenSandbox();
        void Fail(std::string message);

        const FS::FileSystemManager& m_FileSystem;
        sol::state                   m_Lua;
        // debug.traceback, taken before the debug library is removed.
        sol::protected_function      m_Traceback;
        std::string                  m_LastError;
    };
}

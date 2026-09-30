#include "HedgehogScripting/api/ScriptVM.hpp"

#include "Bindings/Bindings.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "Logger/api/Logger.hpp"

#include <exception>
#include <utility>

namespace HedgehogScripting
{
    namespace
    {
        int s_OpenStates = 0;

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
    }

    ScriptVM::ScriptVM(const FS::FileSystemManager& fileSystem)
        : m_FileSystem(fileSystem)
    {
        ++s_OpenStates;
        OpenSandbox();
    }

    ScriptVM::~ScriptVM()
    {
        --s_OpenStates;
    }

    void ScriptVM::OpenSandbox()
    {
        m_Lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table,
                             sol::lib::coroutine, sol::lib::utf8, sol::lib::os, sol::lib::debug);

        m_Traceback = m_Lua["debug"]["traceback"];

        const sol::protected_function_result result =
            m_Lua.safe_script(SANDBOX_SETUP, sol::script_pass_on_error, "=sandbox setup");
        if (!result.valid())
        {
            const sol::error error = result;
            Fail(std::string("ScriptVM: sandbox setup failed: ") + error.what());
        }

        Bindings::RegisterMath(m_Lua);
        Bindings::RegisterLog(m_Lua);
    }

    std::optional<sol::protected_function> ScriptVM::Load(std::string_view source, const std::string& chunkName)
    {
        try
        {
            sol::load_result loaded = m_Lua.load(source, chunkName, sol::load_mode::text);
            if (!loaded.valid())
            {
                const sol::error error = loaded;
                Fail(error.what());
                return std::nullopt;
            }
            sol::protected_function function = loaded;
            function.set_error_handler(m_Traceback);
            return function;
        }
        catch (const std::exception& e)
        {
            Fail(std::string("ScriptVM::Load '") + chunkName + "': " + e.what());
            return std::nullopt;
        }
    }

    std::optional<sol::protected_function> ScriptVM::LoadFile(const std::string& virtualPath)
    {
        const std::optional<std::string> source = m_FileSystem.ReadTextFile(virtualPath);
        if (!source)
        {
            Fail("ScriptVM::LoadFile: cannot read '" + virtualPath + "'.");
            return std::nullopt;
        }
        return Load(*source, "@" + virtualPath);
    }

    bool ScriptVM::Run(std::string_view source, const std::string& chunkName)
    {
        const std::optional<sol::protected_function> chunk = Load(source, chunkName);
        return chunk && Call(*chunk);
    }

    bool ScriptVM::RunFile(const std::string& virtualPath)
    {
        const std::optional<sol::protected_function> chunk = LoadFile(virtualPath);
        return chunk && Call(*chunk);
    }

    sol::state& ScriptVM::GetState()
    {
        return m_Lua;
    }

    const std::string& ScriptVM::GetLastError() const
    {
        return m_LastError;
    }

    const sol::protected_function& ScriptVM::GetTracebackHandler() const
    {
        return m_Traceback;
    }

    int ScriptVM::GetOpenStateCount()
    {
        return s_OpenStates;
    }

    void ScriptVM::Fail(std::string message)
    {
        LOGERROR("[Lua Error] ", message);
        m_LastError = std::move(message);
    }
}

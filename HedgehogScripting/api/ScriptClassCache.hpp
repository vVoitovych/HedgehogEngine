#pragma once

#include "HedgehogScripting/api/ScriptInstance.hpp"
#include "HedgehogScripting/api/Sol.hpp"

#include "ECS/api/Entity.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace HedgehogScripting
{
    class ScriptVM;

    // Compiles each script file once and hands out instances of it, all in one ScriptVM.
    //
    // A script file defines a class named after its file stem (Player.lua defines
    // Player), usually derived from the base ActorScript. The file runs once, and
    // everything it assigns at top level becomes that class's defaults: the class
    // table itself and plain globals such as `speed = 1.0`.
    //
    // Each instance gets a shallow copy of those defaults as its own globals, so one
    // instance changing `speed` does not touch another. The class table and its
    // methods are shared. Methods reach the calling instance's globals because every
    // class file is loaded with a proxy environment that forwards to whichever
    // instance is being called. A consequence: a coroutine resumed from inside
    // another instance's call sees that instance's globals.
    class ScriptClassCache
    {
    public:
        static constexpr const char* DEFAULT_BASE_SCRIPT = "assets://Scripts/Base/ActorScript.lua";

        // Loads the base script at once; if that fails, every CreateInstance fails.
        explicit ScriptClassCache(ScriptVM& vm, std::string baseScriptPath = DEFAULT_BASE_SCRIPT);

        ScriptClassCache(const ScriptClassCache&)            = delete;
        ScriptClassCache& operator=(const ScriptClassCache&) = delete;

        // Compiles scriptPath on first use, then makes an instance by calling the
        // class's new(). Errors are logged in the "[Script] <entity> (<file>)" form.
        [[nodiscard]] std::optional<ScriptInstance> CreateInstance(const std::string& scriptPath,
                                                                   ECS::Entity entity,
                                                                   const std::string& entityName);

        // The file's top-level globals after it ran once (compiling it if needed):
        // its class table and its default values. Nothing is instantiated.
        [[nodiscard]] std::optional<sol::table> GetDefaults(const std::string& scriptPath);

        [[nodiscard]] bool IsBaseLoaded() const;
        // Script files compiled so far, the base included. A cached file is never recompiled.
        [[nodiscard]] int GetCompileCount() const;

    private:
        struct ScriptClass
        {
            sol::table Defaults; // the file's top-level globals, the class table among them
            sol::table Class;
        };

        const ScriptClass* FindOrCompile(const std::string& scriptPath, const std::string& entityName);
        void               LogError(const std::string& entityName, const std::string& scriptPath,
                                    const std::string& message) const;

        ScriptVM&   m_VM;
        std::string m_BaseScriptPath;
        bool        m_BaseLoaded = false;
        int         m_CompileCount = 0;

        sol::environment        m_BaseEnvironment; // where the base script lives; falls back to the sandbox
        sol::environment        m_Proxy;           // every class file's _ENV
        sol::protected_function m_Run;             // run(env, chunk): runs a chunk with env current
        sol::protected_function m_Invoke;          // invoke(env, target, method, ...)
        sol::protected_function m_NewEnvironment;  // newEnvironment(defaults or nil)

        std::unordered_map<std::string, std::unique_ptr<ScriptClass>> m_Classes;
    };
}

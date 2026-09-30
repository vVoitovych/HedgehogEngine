#pragma once

#include "HedgehogScripting/api/Sol.hpp"

#include "ECS/api/Entity.hpp"
#include "ECS/api/System.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ECS
{
    class ECS;
}

namespace FS
{
    class FileSystemManager;
}

namespace HedgehogEngine
{
    class EngineContext;
}

namespace HedgehogScripting
{
    // The script system (ADR-022): runs the Lua script of every entity with a ScriptComponent and
    // a TransformComponent. The component holds only data; every piece of runtime state lives
    // here: one sandboxed Lua state, each script file's class compiled once per Play, and each
    // entity's private environment and instance.
    //
    // A script file defines a class named after its stem (Scripts/Player.lua defines Player),
    // usually derived from ActorScript (Scripts/Base/ActorScript.lua, run once in a base
    // environment). The file's top-level globals are the class defaults: every entity gets its
    // own shallow copy of them, so `speed = 1.0` is per entity while the methods are shared.
    //
    // Driven by the ECS play-mode events: OnPlayStart makes an instance for every enabled entity
    // with a script, OnUpdate calls the script's OnStart once and then OnUpdate(dt), and
    // OnPlayStop calls OnDestroy and drops them all. A script error is logged once as
    // "[Script] <entity> (<assets://path>): <message>" with a traceback; that script stops
    // updating, and the others carry on.
    class ScriptSystem : public ECS::System
    {
    public:
        // scriptFiles resolves "assets://" script paths; it must outlive the system.
        explicit ScriptSystem(const FS::FileSystemManager& scriptFiles);
        ~ScriptSystem() override;

        ScriptSystem(const ScriptSystem&)            = delete;
        ScriptSystem& operator=(const ScriptSystem&) = delete;

        void OnPlayStart(ECS::ECS& ecs) override;
        void OnPlayStop(ECS::ECS& ecs) override;
        void OnUpdate(ECS::ECS& ecs, float deltaTime) override;

        // Entities with a live script instance.
        [[nodiscard]] size_t GetScriptCount() const;
        // Script files compiled since construction, the base script included.
        [[nodiscard]] int GetCompileCount() const;
        // The entity whose script is running right now, if any.
        [[nodiscard]] std::optional<ECS::Entity> GetRunningEntity() const;

        // "Scripts\\Player.lua", "Scripts/Player.lua" and "assets://Scripts/Player.lua" all name
        // "assets://Scripts/Player.lua".
        [[nodiscard]] static std::string NormalizeScriptPath(std::string_view scriptPath);

    private:
        // A compiled script file: its top-level globals and its class table.
        struct ScriptClass
        {
            sol::table Defaults;
            sol::table Class;
        };

        // One entity's script instance.
        struct EntityScript
        {
            std::string ScriptPath;
            uint32_t    Generation = 0;
            std::string EntityName;
            sol::table  Environment;
            sol::table  Self;
            bool        Started = false;
            bool        Faulted = false;
        };

        void               StartClassSupport();
        bool               EnsureBaseLoaded();
        const ScriptClass* FindOrCompile(const std::string& scriptPath, const std::string& entityName);
        void               CreateScript(ECS::ECS& ecs, ECS::Entity entity);
        // Calls self:method(args...) with the entity's environment current. Returns false, having
        // logged the error, when the call fails.
        template<typename... Args>
        bool Invoke(ECS::Entity entity, EntityScript& script, std::string_view method, Args&&... args);
        void LogError(const std::string& entityName, const std::string& scriptPath, const std::string& message) const;

        const FS::FileSystemManager& m_ScriptFiles;

        sol::state              m_Lua;
        sol::protected_function m_Traceback;
        sol::environment        m_BaseEnvironment;
        bool                    m_BaseLoaded = false;

        // Class support (see ScriptSystem.cpp): the proxy _ENV every class file runs under, and
        // the functions that switch which environment it forwards to.
        sol::environment        m_Proxy;
        sol::protected_function m_Run;
        sol::protected_function m_Invoke;
        sol::protected_function m_NewEnvironment;

        std::unordered_map<std::string, ScriptClass>  m_Classes;
        std::unordered_map<ECS::Entity, EntityScript> m_Scripts;
        std::optional<ECS::Entity>                    m_RunningEntity;
        int                                           m_CompileCount = 0;
    };

    // Registers the script system in the engine's ECS with its signature (ScriptComponent and
    // TransformComponent) and returns it. The one place a ScriptSystem is built: the Editor, game
    // mode and the tests all call it. scriptFiles resolves "assets://" and must outlive the
    // system; tests point it at a temporary directory.
    [[nodiscard]] std::shared_ptr<ScriptSystem> RegisterScriptSystem(HedgehogEngine::EngineContext& context,
                                                                     const FS::FileSystemManager&   scriptFiles);
}

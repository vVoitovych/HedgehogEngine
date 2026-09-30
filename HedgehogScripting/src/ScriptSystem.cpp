#include "HedgehogScripting/api/ScriptSystem.hpp"

#include "Sandbox.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <utility>
#include <vector>

namespace HedgehogScripting
{
    namespace
    {
        constexpr std::string_view ASSETS_PREFIX    = "assets://";
        constexpr std::string_view BASE_SCRIPT_PATH = "assets://Scripts/Base/ActorScript.lua";

        // Called with the base environment; returns proxy, run, invoke and newEnvironment.
        // Every class file runs under `proxy` as its _ENV, and the proxy forwards to `current`:
        // the environment of the entity being called, or of the class file being compiled.
        // So methods are shared between entities, but the globals they touch are the calling
        // entity's own.
        constexpr std::string_view CLASS_SUPPORT = R"lua(
            local fallback = ...
            local current = nil

            local proxy = setmetatable({}, {
                __index = function(_, key) return (current or fallback)[key] end,
                __newindex = function(_, key, value) (current or fallback)[key] = value end,
            })

            local function restore(previous, ...)
                current = previous
                return ...
            end

            local function run(env, chunk)
                local previous = current
                current = env
                return restore(previous, chunk())
            end

            local function invoke(env, target, method, ...)
                local fn = target[method]
                if fn == nil then
                    return
                end
                local previous = current
                current = env
                return restore(previous, fn(target, ...))
            end

            local function newEnvironment(defaults)
                local env = {}
                if defaults ~= nil then
                    for key, value in pairs(defaults) do
                        env[key] = value
                    end
                end
                return setmetatable(env, { __index = fallback })
            end

            return proxy, run, invoke, newEnvironment
        )lua";

        std::string ClassNameOf(const std::string& scriptPath)
        {
            return std::filesystem::path(scriptPath).stem().string();
        }

        std::string EntityNameOf(ECS::ECS& ecs, ECS::Entity entity)
        {
            if (ecs.HasComponent<ECS::HierarchyComponent>(entity))
                return ecs.GetComponent<ECS::HierarchyComponent>(entity).Name;
            return "Entity " + std::to_string(entity);
        }
    }

    ScriptSystem::ScriptSystem(const FS::FileSystemManager& scriptFiles)
        : m_ScriptFiles(scriptFiles)
    {
        m_Traceback       = OpenSandbox(m_Lua);
        m_BaseEnvironment = sol::environment(m_Lua, sol::create, m_Lua.globals());
        StartClassSupport();
    }

    ScriptSystem::~ScriptSystem()
    {
        // Destroyed mid-Play (the ECS destroys its systems before its component storage): put the
        // engine's removal callback back, since ours points at this system.
        if (m_CallbackEcs != nullptr)
            m_CallbackEcs->SetComponentRemovedCallback<HedgehogEngine::ScriptComponent>(std::move(m_PreviousRemovedCallback));
    }

    void ScriptSystem::StartClassSupport()
    {
        const std::optional<sol::protected_function> support =
            LoadScriptSource(m_Lua, CLASS_SUPPORT, "=script class support", m_Traceback);
        if (!support)
            return;
        try
        {
            const sol::protected_function_result result = (*support)(m_BaseEnvironment);
            if (!result.valid())
            {
                const sol::error error = result;
                ReportScriptError(std::string("script class support failed to start: ") + error.what());
                return;
            }
            m_Proxy          = result.get<sol::environment>(0);
            m_Run            = result.get<sol::protected_function>(1);
            m_Invoke         = result.get<sol::protected_function>(2);
            m_NewEnvironment = result.get<sol::protected_function>(3);
        }
        catch (const std::exception& e)
        {
            ReportScriptError(std::string("script class support failed to start: ") + e.what());
        }
    }

    bool ScriptSystem::EnsureBaseLoaded()
    {
        if (m_BaseLoaded)
            return true;

        // The base script defines ActorScript in the base environment, where every class file's
        // lookups end up.
        const std::optional<sol::protected_function> base =
            LoadScriptFile(m_Lua, m_ScriptFiles, std::string(BASE_SCRIPT_PATH), m_Traceback);
        if (!base)
            return false;
        ++m_CompileCount;
        sol::set_environment(m_BaseEnvironment, *base);
        m_BaseLoaded = CallProtected(*base);
        return m_BaseLoaded;
    }

    const ScriptSystem::ScriptClass* ScriptSystem::FindOrCompile(const std::string& scriptPath,
                                                                const std::string& entityName)
    {
        if (const auto it = m_Classes.find(scriptPath); it != m_Classes.end())
            return &it->second;

        const std::optional<sol::protected_function> chunk =
            LoadScriptFile(m_Lua, m_ScriptFiles, scriptPath, m_Traceback);
        if (!chunk)
            return nullptr;
        ++m_CompileCount;
        sol::set_environment(m_Proxy, *chunk);

        try
        {
            sol::table defaults = m_NewEnvironment().get<sol::table>();
            const sol::protected_function_result ran = m_Run(defaults, *chunk);
            if (!ran.valid())
            {
                const sol::error error = ran;
                LogError(entityName, scriptPath, error.what());
                return nullptr;
            }

            const std::string className   = ClassNameOf(scriptPath);
            const sol::object classObject = defaults.raw_get<sol::object>(className);
            if (classObject.get_type() != sol::type::table)
            {
                LogError(entityName, scriptPath, "the script does not define the class '" + className + "'.");
                return nullptr;
            }

            const auto [it, inserted] = m_Classes.emplace(scriptPath, ScriptClass{ defaults, classObject.as<sol::table>() });
            return &it->second;
        }
        catch (const std::exception& e)
        {
            LogError(entityName, scriptPath, e.what());
            return nullptr;
        }
    }

    void ScriptSystem::CreateScript(ECS::ECS& ecs, ECS::Entity entity)
    {
        const auto& component = ecs.GetComponent<HedgehogEngine::ScriptComponent>(entity);
        if (component.ScriptPath.empty())
            return;

        // The entry exists from here on, so a script that fails to load is not retried every
        // frame: it stays faulted until the next Play.
        EntityScript& script = m_Scripts[entity];
        script            = EntityScript{};
        script.ScriptPath = NormalizeScriptPath(component.ScriptPath);
        script.Generation = ecs.GetGeneration(entity);
        script.EntityName = EntityNameOf(ecs, entity);
        script.Faulted    = true;

        if (!EnsureBaseLoaded())
        {
            LogError(script.EntityName, script.ScriptPath,
                     "the base script '" + std::string(BASE_SCRIPT_PATH) + "' is not loaded.");
            return;
        }

        const ScriptClass* scriptClass = FindOrCompile(script.ScriptPath, script.EntityName);
        if (scriptClass == nullptr)
            return;

        try
        {
            script.Environment = m_NewEnvironment(scriptClass->Defaults).get<sol::table>();
            ApplyParams(ecs, entity, script);

            m_RunningEntity = entity;
            const sol::protected_function_result made =
                m_Invoke(script.Environment, scriptClass->Class, "new");
            m_RunningEntity.reset();
            if (!made.valid())
            {
                const sol::error error = made;
                LogError(script.EntityName, script.ScriptPath, error.what());
                return;
            }
            if (made.get_type() != sol::type::table)
            {
                LogError(script.EntityName, script.ScriptPath,
                         ClassNameOf(script.ScriptPath) + ":new() did not return a table.");
                return;
            }
            script.Self    = made.get<sol::table>();
            script.Faulted = false;
        }
        catch (const std::exception& e)
        {
            m_RunningEntity.reset();
            LogError(script.EntityName, script.ScriptPath, e.what());
        }
    }

    void ScriptSystem::ApplyParams(ECS::ECS& ecs, ECS::Entity entity, EntityScript& script)
    {
        const auto& component = ecs.GetComponent<HedgehogEngine::ScriptComponent>(entity);
        for (const auto& [name, param] : component.Params)
        {
            switch (param.type)
            {
            case HedgehogEngine::ParamType::Boolean:
                script.Environment[name] = std::get<bool>(param.value);
                break;
            case HedgehogEngine::ParamType::Number:
                script.Environment[name] = std::get<float>(param.value);
                break;
            default:
                break;
            }
        }
    }

    template<typename... Args>
    bool ScriptSystem::Invoke(ECS::Entity entity, EntityScript& script, std::string_view method, Args&&... args)
    {
        // Copies, so the call is safe even if the script's entry changes while it runs.
        const sol::table  environment = script.Environment;
        const sol::table  self        = script.Self;
        const std::string entityName  = script.EntityName;
        const std::string scriptPath  = script.ScriptPath;

        m_RunningEntity = entity;
        bool succeeded  = false;
        try
        {
            const sol::protected_function_result result =
                m_Invoke(environment, self, method, std::forward<Args>(args)...);
            succeeded = result.valid();
            if (!succeeded)
            {
                const sol::error error = result;
                LogError(entityName, scriptPath, error.what());
            }
        }
        catch (const std::exception& e)
        {
            LogError(entityName, scriptPath, e.what());
        }
        m_RunningEntity.reset();
        return succeeded;
    }

    template<typename... Args>
    void ScriptSystem::InvokeAll(std::string_view method, Args&&... args)
    {
        // A script may change the set of scripts while it runs, so walk a sorted snapshot of the
        // ids and look each one up again.
        std::vector<ECS::Entity> entities;
        entities.reserve(m_Scripts.size());
        for (const auto& [entity, script] : m_Scripts)
            entities.push_back(entity);
        std::sort(entities.begin(), entities.end());

        for (const ECS::Entity entity : entities)
        {
            const auto it = m_Scripts.find(entity);
            if (it == m_Scripts.end() || !it->second.Enabled || it->second.Faulted)
                continue;
            if (!Invoke(entity, it->second, method, args...))
            {
                if (const auto failed = m_Scripts.find(entity); failed != m_Scripts.end())
                    failed->second.Faulted = true;
            }
        }
    }

    void ScriptSystem::SyncScripts(ECS::ECS& ecs)
    {
        // Entities that left the system (lost their Transform) or whose id was recycled.
        std::vector<ECS::Entity> gone;
        for (const auto& [entity, script] : m_Scripts)
        {
            const bool member = std::find(m_Entities.begin(), m_Entities.end(), entity) != m_Entities.end();
            if (!member || !ecs.IsAlive(entity) || ecs.GetGeneration(entity) != script.Generation)
                gone.push_back(entity);
        }
        std::sort(gone.begin(), gone.end());
        for (const ECS::Entity entity : gone)
            DestroyScript(entity);

        // Scripts added during Play.
        std::vector<ECS::Entity> members(m_Entities.begin(), m_Entities.end());
        std::sort(members.begin(), members.end());
        for (const ECS::Entity entity : members)
        {
            if (m_Scripts.find(entity) == m_Scripts.end())
                CreateScript(ecs, entity);
        }

        // Enable changes: OnStart once on the first enable, then OnEnable; OnDisable when turned off.
        for (const ECS::Entity entity : members)
        {
            const auto it = m_Scripts.find(entity);
            if (it == m_Scripts.end() || it->second.Faulted)
                continue;
            const bool wanted = ecs.GetComponent<HedgehogEngine::ScriptComponent>(entity).Enable;
            if (wanted == it->second.Enabled)
                continue;

            it->second.Enabled = wanted;
            bool succeeded     = true;
            if (wanted)
            {
                if (!it->second.Started)
                {
                    it->second.Started = true;
                    succeeded          = Invoke(entity, it->second, "OnStart");
                }
                if (succeeded)
                    succeeded = Invoke(entity, m_Scripts.at(entity), "OnEnable");
            }
            else
            {
                (void)Invoke(entity, it->second, "OnDisable");
            }
            if (!succeeded)
            {
                if (const auto failed = m_Scripts.find(entity); failed != m_Scripts.end())
                    failed->second.Faulted = true;
            }
        }
    }

    void ScriptSystem::DestroyScript(ECS::Entity entity)
    {
        const auto it = m_Scripts.find(entity);
        if (it == m_Scripts.end())
            return;

        EntityScript script = std::move(it->second);
        m_Scripts.erase(it);
        if (!script.Self.valid())
            return;
        if (script.Enabled && !script.Faulted)
            (void)Invoke(entity, script, "OnDisable");
        (void)Invoke(entity, script, "OnDestroy");
    }

    void ScriptSystem::OnScriptRemoved(ECS::Entity entity)
    {
        // A script whose own call is running cannot be torn down under it: finish that first.
        if (m_RunningEntity.has_value())
        {
            m_PendingRemovals.push_back(entity);
            return;
        }
        DestroyScript(entity);
    }

    void ScriptSystem::OnPlayStart(ECS::ECS& ecs)
    {
        // Every Play reads the script files afresh, so an edit made in Edit mode takes effect.
        m_Classes.clear();
        m_Scripts.clear();
        m_PendingRemovals.clear();

        // Chain in front of whatever removal callback the engine keeps for ScriptComponent, for
        // the length of Play.
        if (m_CallbackEcs == nullptr)
        {
            m_CallbackEcs             = &ecs;
            m_PreviousRemovedCallback = ecs.GetComponentRemovedCallback<HedgehogEngine::ScriptComponent>();
            ecs.SetComponentRemovedCallback<HedgehogEngine::ScriptComponent>(
                [this, previous = m_PreviousRemovedCallback](ECS::Entity entity, HedgehogEngine::ScriptComponent& component)
                {
                    OnScriptRemoved(entity);
                    if (previous)
                        previous(entity, component);
                });
        }

        std::vector<ECS::Entity> members(m_Entities.begin(), m_Entities.end());
        std::sort(members.begin(), members.end());
        for (const ECS::Entity entity : members)
            CreateScript(ecs, entity);
    }

    void ScriptSystem::OnFixedUpdate(ECS::ECS& ecs, float fixedDeltaTime)
    {
        SyncScripts(ecs);
        InvokeAll("OnFixedUpdate", fixedDeltaTime);
        for (const ECS::Entity entity : std::exchange(m_PendingRemovals, {}))
            DestroyScript(entity);
    }

    void ScriptSystem::OnUpdate(ECS::ECS& ecs, float deltaTime)
    {
        SyncScripts(ecs);
        InvokeAll("OnUpdate", deltaTime);
        for (const ECS::Entity entity : std::exchange(m_PendingRemovals, {}))
            DestroyScript(entity);
    }

    void ScriptSystem::OnPlayStop(ECS::ECS& ecs)
    {
        std::vector<ECS::Entity> entities;
        entities.reserve(m_Scripts.size());
        for (const auto& [entity, script] : m_Scripts)
            entities.push_back(entity);
        std::sort(entities.begin(), entities.end());
        for (const ECS::Entity entity : entities)
            DestroyScript(entity);
        m_Scripts.clear();
        m_PendingRemovals.clear();

        if (m_CallbackEcs == &ecs)
        {
            ecs.SetComponentRemovedCallback<HedgehogEngine::ScriptComponent>(std::move(m_PreviousRemovedCallback));
            m_PreviousRemovedCallback = {};
            m_CallbackEcs             = nullptr;
        }
    }

    void ScriptSystem::PushParams(ECS::ECS& ecs, ECS::Entity entity)
    {
        const auto it = m_Scripts.find(entity);
        if (it == m_Scripts.end() || !it->second.Environment.valid())
            return;
        ApplyParams(ecs, entity, it->second);
    }

    std::unordered_map<std::string, HedgehogEngine::ScriptParam> ScriptSystem::DescribeScript(const std::string& scriptPath)
    {
        std::unordered_map<std::string, HedgehogEngine::ScriptParam> params;
        const std::string path = NormalizeScriptPath(scriptPath);
        if (!EnsureBaseLoaded())
            return params;

        const std::optional<sol::protected_function> chunk = LoadScriptFile(m_Lua, m_ScriptFiles, path, m_Traceback);
        if (!chunk)
            return params;
        sol::set_environment(m_Proxy, *chunk);

        try
        {
            // Runs the file's top level into a throwaway table, as a Play compile would, and
            // calls no method.
            sol::table defaults = m_NewEnvironment().get<sol::table>();
            const sol::protected_function_result ran = m_Run(defaults, *chunk);
            if (!ran.valid())
            {
                const sol::error error = ran;
                LogError("DescribeScript", path, error.what());
                return params;
            }
            for (const auto& [key, value] : defaults)
            {
                if (key.get_type() != sol::type::string)
                    continue;
                if (value.get_type() == sol::type::number)
                    params[key.as<std::string>()] = { HedgehogEngine::ParamType::Number, value.as<float>(), false };
                else if (value.get_type() == sol::type::boolean)
                    params[key.as<std::string>()] = { HedgehogEngine::ParamType::Boolean, value.as<bool>(), false };
            }
        }
        catch (const std::exception& e)
        {
            LogError("DescribeScript", path, e.what());
        }
        return params;
    }

    size_t ScriptSystem::GetScriptCount() const
    {
        return m_Scripts.size();
    }

    int ScriptSystem::GetCompileCount() const
    {
        return m_CompileCount;
    }

    std::optional<ECS::Entity> ScriptSystem::GetRunningEntity() const
    {
        return m_RunningEntity;
    }

    std::string ScriptSystem::NormalizeScriptPath(std::string_view scriptPath)
    {
        std::string path(scriptPath);
        std::replace(path.begin(), path.end(), '\\', '/');
        if (path.rfind(ASSETS_PREFIX, 0) != 0)
            path.insert(0, ASSETS_PREFIX);
        return path;
    }

    void ScriptSystem::LogError(const std::string& entityName, const std::string& scriptPath,
                                const std::string& message) const
    {
        LOGERROR("[Script] " + entityName + " (" + scriptPath + "): " + message);
    }

    std::shared_ptr<ScriptSystem> RegisterScriptSystem(HedgehogEngine::EngineContext& context,
                                                       const FS::FileSystemManager&   scriptFiles)
    {
        ECS::ECS& ecs    = context.GetECS();
        auto      system = ecs.RegisterSystem<ScriptSystem>(scriptFiles);

        ECS::Signature signature;
        signature.set(ecs.GetComponentType<HedgehogEngine::ScriptComponent>());
        signature.set(ecs.GetComponentType<HedgehogEngine::TransformComponent>());
        ecs.SetSystemSignature<ScriptSystem>(signature);
        return system;
    }
}

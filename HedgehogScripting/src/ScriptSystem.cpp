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

    ScriptSystem::~ScriptSystem() = default;

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
        if (!component.Enable || component.ScriptPath.empty())
            return;

        const std::string scriptPath = NormalizeScriptPath(component.ScriptPath);
        const std::string entityName = EntityNameOf(ecs, entity);
        if (!EnsureBaseLoaded())
        {
            LogError(entityName, scriptPath, "the base script '" + std::string(BASE_SCRIPT_PATH) + "' is not loaded.");
            return;
        }

        const ScriptClass* scriptClass = FindOrCompile(scriptPath, entityName);
        if (scriptClass == nullptr)
            return;

        EntityScript script;
        script.ScriptPath = scriptPath;
        script.Generation = ecs.GetGeneration(entity);
        script.EntityName = entityName;
        try
        {
            script.Environment = m_NewEnvironment(scriptClass->Defaults).get<sol::table>();

            m_RunningEntity = entity;
            const sol::protected_function_result made =
                m_Invoke(script.Environment, scriptClass->Class, "new");
            m_RunningEntity.reset();
            if (!made.valid())
            {
                const sol::error error = made;
                LogError(entityName, scriptPath, error.what());
                return;
            }
            if (made.get_type() != sol::type::table)
            {
                LogError(entityName, scriptPath, ClassNameOf(scriptPath) + ":new() did not return a table.");
                return;
            }
            script.Self = made.get<sol::table>();
        }
        catch (const std::exception& e)
        {
            m_RunningEntity.reset();
            LogError(entityName, scriptPath, e.what());
            return;
        }

        m_Scripts.insert_or_assign(entity, std::move(script));
    }

    template<typename... Args>
    bool ScriptSystem::Invoke(ECS::Entity entity, EntityScript& script, std::string_view method, Args&&... args)
    {
        m_RunningEntity = entity;
        bool succeeded  = false;
        try
        {
            const sol::protected_function_result result =
                m_Invoke(script.Environment, script.Self, method, std::forward<Args>(args)...);
            succeeded = result.valid();
            if (!succeeded)
            {
                const sol::error error = result;
                LogError(script.EntityName, script.ScriptPath, error.what());
            }
        }
        catch (const std::exception& e)
        {
            LogError(script.EntityName, script.ScriptPath, e.what());
        }
        m_RunningEntity.reset();
        return succeeded;
    }

    void ScriptSystem::OnPlayStart(ECS::ECS& ecs)
    {
        // Every Play reads the script files afresh, so an edit made in Edit mode takes effect.
        m_Classes.clear();
        m_Scripts.clear();
        for (const ECS::Entity entity : m_Entities)
            CreateScript(ecs, entity);
    }

    void ScriptSystem::OnUpdate(ECS::ECS& ecs, float deltaTime)
    {
        // A script may change the set of scripts while it runs, so walk a snapshot of the ids.
        std::vector<ECS::Entity> entities;
        entities.reserve(m_Scripts.size());
        for (const auto& [entity, script] : m_Scripts)
            entities.push_back(entity);
        std::sort(entities.begin(), entities.end());

        for (const ECS::Entity entity : entities)
        {
            const auto it = m_Scripts.find(entity);
            if (it == m_Scripts.end())
                continue;
            EntityScript& script = it->second;
            if (!ecs.IsAlive(entity) || ecs.GetGeneration(entity) != script.Generation)
            {
                m_Scripts.erase(it);
                continue;
            }
            if (script.Faulted)
                continue;

            if (!script.Started)
            {
                script.Started = true;
                if (!Invoke(entity, script, "OnStart"))
                {
                    script.Faulted = true;
                    continue;
                }
            }
            if (!Invoke(entity, script, "OnUpdate", deltaTime))
                script.Faulted = true;
        }
    }

    void ScriptSystem::OnPlayStop(ECS::ECS& /*ecs*/)
    {
        for (auto& [entity, script] : m_Scripts)
            (void)Invoke(entity, script, "OnDestroy");
        m_Scripts.clear();
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

#include "HedgehogScripting/api/ScriptSystem.hpp"

#include "Sandbox.hpp"
#include "Bindings/ScriptHandles.hpp"

#include "ECS/api/ECS.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

// Hot reload: a script file saved while its scripts run is compiled again and every entity running
// it moves onto the new class, keeping its state. See ScriptSystem::ReloadChangedScripts.
namespace HedgehogScripting
{
    namespace
    {
        // Returns copyState(from, to): copies from's entries but `entity` into to. Numbers,
        // booleans, strings and userdata (Vector3, Quat, Entity and component handles) are
        // copied as they are; tables are copied deeply, each one once, so a cycle stays a cycle
        // and never loops, and keep their metatables; functions and threads are old code and
        // are dropped, as are entries whose key is one of them.
        constexpr std::string_view RELOAD_SUPPORT = R"lua(
            local function copyState(from, to)
                local seen = {}
                local function copy(value)
                    local kind = type(value)
                    if kind == "function" or kind == "thread" then
                        return nil
                    end
                    if kind ~= "table" then
                        return value
                    end
                    if seen[value] ~= nil then
                        return seen[value]
                    end
                    local result = {}
                    seen[value] = result
                    for key, item in pairs(value) do
                        local copiedKey, copiedItem = copy(key), copy(item)
                        if copiedKey ~= nil and copiedItem ~= nil then
                            result[copiedKey] = copiedItem
                        end
                    end
                    return setmetatable(result, getmetatable(value))
                end

                for key, value in pairs(from) do
                    if key ~= "entity" then
                        local copied = copy(value)
                        if copied ~= nil then
                            to[key] = copied
                        end
                    end
                end
            end

            return copyState
        )lua";
    }

    void ScriptSystem::RegisterReload()
    {
        const std::optional<sol::protected_function> support =
            LoadScriptSource(m_Lua, RELOAD_SUPPORT, "=script reload support", m_Traceback);
        if (!support)
            return;
        try
        {
            const sol::protected_function_result result = (*support)();
            if (!result.valid())
            {
                const sol::error error = result;
                ReportScriptError(std::string("script reload support failed to start: ") + error.what());
                return;
            }
            m_CopyState = result.get<sol::protected_function>(0);
        }
        catch (const std::exception& e)
        {
            ReportScriptError(std::string("script reload support failed to start: ") + e.what());
        }
    }

    void ScriptSystem::ReloadChangedScripts(ECS::ECS& ecs, std::chrono::steady_clock::time_point now)
    {
        if (m_LastReloadPoll && now - *m_LastReloadPoll < RELOAD_POLL_INTERVAL)
            return;
        m_LastReloadPoll = now;

        // FileSystem has no write-time query, so the compiled class keeps its physical path.
        std::vector<std::string> changed;
        for (const auto& [scriptPath, scriptClass] : m_Classes)
        {
            if (scriptClass.PhysicalPath.empty())
                continue;
            std::error_code error;
            const auto      writeTime = std::filesystem::last_write_time(scriptClass.PhysicalPath, error);
            if (!error && writeTime != scriptClass.WriteTime)
                changed.push_back(scriptPath);
        }
        std::sort(changed.begin(), changed.end());
        for (const std::string& scriptPath : changed)
            ReloadClass(ecs, scriptPath);
    }

    void ScriptSystem::ReloadClass(ECS::ECS& ecs, const std::string& scriptPath)
    {
        std::vector<ECS::Entity> users;
        for (const auto& [entity, script] : m_Scripts)
            if (script.ScriptPath == scriptPath)
                users.push_back(entity);
        std::sort(users.begin(), users.end());

        // Nothing runs it (Edit mode, say): forget it, so it is compiled afresh when next needed.
        if (users.empty())
        {
            m_Classes.erase(scriptPath);
            return;
        }

        std::optional<ScriptClass> compiled = CompileClass(scriptPath, m_Scripts.at(users.front()).EntityName);
        if (!compiled)
        {
            // CompileClass logged why. The old class keeps running, and this version of the
            // file is not tried again: the next save is.
            ScriptClass&    old = m_Classes.at(scriptPath);
            std::error_code error;
            const auto      writeTime = std::filesystem::last_write_time(old.PhysicalPath, error);
            if (!error)
                old.WriteTime = writeTime;
            return;
        }

        const ScriptClass& scriptClass = m_Classes.insert_or_assign(scriptPath, std::move(*compiled)).first->second;
        for (const ECS::Entity entity : users)
            SwapScript(ecs, entity, scriptClass);
        LOGINFO("[Script] Reloaded " + scriptPath + " for " + std::to_string(users.size()) + " entities.");
    }

    void ScriptSystem::SwapScript(ECS::ECS& ecs, ECS::Entity entity, const ScriptClass& scriptClass)
    {
        const auto found = m_Scripts.find(entity);
        if (found == m_Scripts.end())
            return;

        // A script that never loaded has no state to keep: it starts over, as one added now.
        if (!found->second.Self.valid())
        {
            CreateScript(ecs, entity);
            return;
        }

        const sol::table  oldSelf    = found->second.Self;
        const std::string entityName = found->second.EntityName;
        const std::string scriptPath = found->second.ScriptPath;
        try
        {
            sol::table environment = m_NewEnvironment(scriptClass.Defaults).get<sol::table>();
            m_RunningEntity = entity;
            const sol::protected_function_result made = m_Invoke(environment, scriptClass.Class, "new");
            m_RunningEntity.reset();
            if (!made.valid() || made.get_type() != sol::type::table)
            {
                if (made.valid())
                {
                    LogError(entityName, scriptPath, "new() did not return a table on reload.");
                }
                else
                {
                    const sol::error error = made;
                    LogError(entityName, scriptPath, error.what());
                }
                if (const auto failed = m_Scripts.find(entity); failed != m_Scripts.end())
                {
                    failed->second.Faulted = true;
                    failed->second.Coroutines.clear();
                }
                return;
            }

            sol::table self = made.get<sol::table>();
            self["entity"]  = Bindings::MakeScriptEntity(ecs, entity);
            if (const sol::protected_function_result copied = m_CopyState(oldSelf, self); !copied.valid())
            {
                const sol::error error = copied;
                LogError(entityName, scriptPath, std::string("copying state on reload: ") + error.what());
            }

            // Looked up again: new() may have changed the scripts.
            const auto script = m_Scripts.find(entity);
            if (script == m_Scripts.end())
                return;
            script->second.Environment = environment;
            script->second.Self        = self;
            script->second.Faulted     = false;
            script->second.Coroutines.clear();
            ApplyProperties(ecs, entity, script->second, false);
        }
        catch (const std::exception& e)
        {
            m_RunningEntity.reset();
            LogError(entityName, scriptPath, e.what());
            return;
        }

        if (const auto script = m_Scripts.find(entity); script != m_Scripts.end())
        {
            if (!Invoke(entity, script->second, "OnReload"))
            {
                if (const auto failed = m_Scripts.find(entity); failed != m_Scripts.end())
                    failed->second.Faulted = true;
            }
        }
    }
}

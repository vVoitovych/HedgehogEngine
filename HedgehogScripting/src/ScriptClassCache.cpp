#include "HedgehogScripting/api/ScriptClassCache.hpp"
#include "HedgehogScripting/api/ScriptVM.hpp"

#include "Logger/api/Logger.hpp"

#include <exception>
#include <filesystem>
#include <utility>

namespace HedgehogScripting
{
    namespace
    {
        // Called with the base environment; returns proxy, run, invoke, newEnvironment.
        // `current` is the environment of the instance (or class file) being run.
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
    }

    ScriptClassCache::ScriptClassCache(ScriptVM& vm, std::string baseScriptPath)
        : m_VM(vm)
        , m_BaseScriptPath(std::move(baseScriptPath))
    {
        sol::state& lua = m_VM.GetState();
        m_BaseEnvironment = sol::environment(lua, sol::create, lua.globals());

        const std::optional<sol::protected_function> support = m_VM.Load(CLASS_SUPPORT, "=script class support");
        if (!support)
            return;
        try
        {
            sol::protected_function withTraceback = *support;
            withTraceback.set_error_handler(m_VM.GetTracebackHandler());
            const sol::protected_function_result result = withTraceback(m_BaseEnvironment);
            if (!result.valid())
            {
                const sol::error error = result;
                LOGERROR("[Script] class support failed to start: ", error.what());
                return;
            }
            m_Proxy          = result.get<sol::environment>(0);
            m_Run            = result.get<sol::protected_function>(1);
            m_Invoke         = result.get<sol::protected_function>(2);
            m_NewEnvironment = result.get<sol::protected_function>(3);
            m_Invoke.set_error_handler(m_VM.GetTracebackHandler());
        }
        catch (const std::exception& e)
        {
            LOGERROR("[Script] class support failed to start: ", e.what());
            return;
        }

        LoadBase();
    }

    void ScriptClassCache::LoadBase()
    {
        // The base script defines ActorScript in the base environment, where every
        // class file's lookups end up.
        m_BaseLoaded = false;
        if (!m_Proxy.valid())
            return;
        const std::optional<sol::protected_function> base = m_VM.LoadFile(m_BaseScriptPath);
        if (!base)
            return;
        ++m_CompileCount;
        sol::set_environment(m_BaseEnvironment, *base);
        m_BaseLoaded = m_VM.Call(*base);
    }

    void ScriptClassCache::Reload()
    {
        m_Classes.clear();
        LoadBase();
    }

    std::optional<ScriptInstance> ScriptClassCache::CreateInstance(const std::string& scriptPath,
                                                                   ECS::Entity entity,
                                                                   const std::string& entityName)
    {
        if (!m_BaseLoaded)
        {
            LogError(entityName, scriptPath, "the base script '" + m_BaseScriptPath + "' is not loaded.");
            return std::nullopt;
        }

        const ScriptClass* scriptClass = FindOrCompile(scriptPath, entityName);
        if (scriptClass == nullptr)
            return std::nullopt;

        try
        {
            sol::protected_function newEnvironment = m_NewEnvironment;
            const sol::protected_function_result env = newEnvironment(scriptClass->Defaults);
            if (!env.valid())
            {
                const sol::error error = env;
                LogError(entityName, scriptPath, error.what());
                return std::nullopt;
            }
            sol::table environment = env.get<sol::table>();

            const sol::protected_function_result made = m_Invoke(environment, scriptClass->Class, "new");
            if (!made.valid())
            {
                const sol::error error = made;
                LogError(entityName, scriptPath, error.what());
                return std::nullopt;
            }
            if (made.get_type() != sol::type::table)
            {
                LogError(entityName, scriptPath, ClassNameOf(scriptPath) + ":new() did not return a table.");
                return std::nullopt;
            }

            return ScriptInstance(entity, entityName, scriptPath, std::move(environment), made.get<sol::table>(),
                                  m_Invoke);
        }
        catch (const std::exception& e)
        {
            LogError(entityName, scriptPath, e.what());
            return std::nullopt;
        }
    }

    const ScriptClassCache::ScriptClass* ScriptClassCache::FindOrCompile(const std::string& scriptPath,
                                                                         const std::string& entityName)
    {
        if (const auto it = m_Classes.find(scriptPath); it != m_Classes.end())
            return it->second.get();

        const std::optional<sol::protected_function> chunk = m_VM.LoadFile(scriptPath);
        if (!chunk)
            return nullptr;
        ++m_CompileCount;
        sol::set_environment(m_Proxy, *chunk);

        try
        {
            sol::protected_function newEnvironment = m_NewEnvironment;
            sol::table defaults = newEnvironment().get<sol::table>();
            if (!m_VM.Call(m_Run, defaults, *chunk))
                return nullptr;

            const std::string className = ClassNameOf(scriptPath);
            const sol::object classObject = defaults.raw_get<sol::object>(className);
            if (classObject.get_type() != sol::type::table)
            {
                LogError(entityName, scriptPath, "the script does not define the class '" + className + "'.");
                return nullptr;
            }

            auto scriptClass      = std::make_unique<ScriptClass>();
            scriptClass->Defaults = defaults;
            scriptClass->Class    = classObject.as<sol::table>();
            return m_Classes.emplace(scriptPath, std::move(scriptClass)).first->second.get();
        }
        catch (const std::exception& e)
        {
            LogError(entityName, scriptPath, e.what());
            return nullptr;
        }
    }

    std::optional<sol::table> ScriptClassCache::GetDefaults(const std::string& scriptPath)
    {
        if (!m_BaseLoaded)
            return std::nullopt;
        const ScriptClass* scriptClass = FindOrCompile(scriptPath, "(describe)");
        if (scriptClass == nullptr)
            return std::nullopt;
        return scriptClass->Defaults;
    }

    bool ScriptClassCache::IsBaseLoaded() const
    {
        return m_BaseLoaded;
    }

    int ScriptClassCache::GetCompileCount() const
    {
        return m_CompileCount;
    }

    void ScriptClassCache::LogError(const std::string& entityName, const std::string& scriptPath,
                                    const std::string& message) const
    {
        LOGERROR("[Script] ", entityName, " (", scriptPath, "): ", message);
    }
}

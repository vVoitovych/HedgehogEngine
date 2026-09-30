#pragma once

#include "HedgehogScripting/api/Sol.hpp"

#include "ECS/api/Entity.hpp"

#include <exception>
#include <string>
#include <string_view>
#include <utility>

namespace HedgehogScripting
{
    class ScriptClassCache;

    // One entity's copy of a script class: its own environment (the script's
    // globals, such as `speed`) and its own `self`, sharing the compiled class with
    // every other instance of the same file. Made by ScriptClassCache::CreateInstance;
    // must not outlive the ScriptVM it was made in.
    class ScriptInstance
    {
    public:
        // Calls self:method(args...) with this instance's globals in scope. A method
        // the class does not define is not an error. On an error the message is
        // logged once as "[Script] <entity> (<file>): <message>" with a traceback,
        // kept as GetLastError(), and false is returned. An error in OnStart or
        // OnUpdate also faults the instance: every later call is skipped (and
        // returns false) until the instance is made again.
        template<typename... Args>
        bool Call(std::string_view method, Args&&... args)
        {
            if (m_Faulted)
                return false;
            try
            {
                const sol::protected_function_result result =
                    m_Invoke(m_Environment, m_Self, method, std::forward<Args>(args)...);
                if (result.valid())
                    return true;
                const sol::error error = result;
                ReportError(method, error.what());
            }
            catch (const std::exception& e)
            {
                ReportError(method, e.what());
            }
            return false;
        }

        [[nodiscard]] bool HasMethod(std::string_view method) const;

        ECS::Entity        GetEntity() const     { return m_Entity; }
        const std::string& GetEntityName() const { return m_EntityName; }
        const std::string& GetScriptPath() const { return m_ScriptPath; }
        bool               IsFaulted() const     { return m_Faulted; }
        const std::string& GetLastError() const  { return m_LastError; }

        // The script's globals for this instance, and the object its methods get as self.
        sol::table& GetEnvironment() { return m_Environment; }
        sol::table& GetSelf()        { return m_Self; }

    private:
        friend class ScriptClassCache;

        ScriptInstance(ECS::Entity entity, std::string entityName, std::string scriptPath,
                       sol::table environment, sol::table self, sol::protected_function invoke);

        void ReportError(std::string_view method, const std::string& message);

        ECS::Entity             m_Entity;
        std::string             m_EntityName;
        std::string             m_ScriptPath;
        sol::table              m_Environment;
        sol::table              m_Self;
        sol::protected_function m_Invoke;
        bool                    m_Faulted = false;
        std::string             m_LastError;
    };
}

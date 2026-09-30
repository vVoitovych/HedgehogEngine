#include "HedgehogScripting/api/ScriptInstance.hpp"

#include "Logger/api/Logger.hpp"

namespace HedgehogScripting
{
    ScriptInstance::ScriptInstance(ECS::Entity entity, std::string entityName, std::string scriptPath,
                                   sol::table environment, sol::table self, sol::protected_function invoke)
        : m_Entity(entity)
        , m_EntityName(std::move(entityName))
        , m_ScriptPath(std::move(scriptPath))
        , m_Environment(std::move(environment))
        , m_Self(std::move(self))
        , m_Invoke(std::move(invoke))
    {
    }

    bool ScriptInstance::HasMethod(std::string_view method) const
    {
        const sol::object function = m_Self[method];
        return function.get_type() == sol::type::function;
    }

    void ScriptInstance::ReportError(std::string_view method, const std::string& message)
    {
        m_LastError = "[Script] " + m_EntityName + " (" + m_ScriptPath + "): " + message;
        LOGERROR(m_LastError);

        if (method == "OnStart" || method == "OnUpdate")
            m_Faulted = true;
    }
}

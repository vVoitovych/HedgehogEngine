#pragma once

#include "ECS/api/Entity.hpp"

#include <optional>
#include <unordered_map>
#include <string>
#include <variant>

struct lua_State;

namespace Scene
{
    enum class ParamType
    {
        Boolean,
        Number
    };

    struct ScriptParam
    {
        ParamType type;
        std::variant<bool, float> value;
        bool dirty = false;
    };

    class ScriptComponent
    {
    public:
        bool                                         m_Enable = true;
        std::optional<bool>                          m_NewEnable;  // runtime-only
        std::string                                  m_ScriptPath;
        std::unordered_map<std::string, ScriptParam> m_Params;

        // Visit serializes simple fields; m_Params is handled manually in EcsSerializer
        template<typename V>
        void Visit(V& v)
        {
            v("ScriptEnable", m_Enable);
            v("ScriptFile",   m_ScriptPath);
        }

        friend class ScriptSystem;

    private:
        lua_State* m_LuaState    = nullptr; // runtime-only
        int        m_InstanceRef = 0;       // runtime-only
    };
}

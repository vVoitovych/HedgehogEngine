#pragma once

#include "ECS/api/Entity.hpp"

#include <optional>
#include <unordered_map>
#include <string>
#include <variant>

struct lua_State;

namespace HedgehogEngine
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
        bool                                         Enable = true;
        std::optional<bool>                          NewEnable;  // runtime-only
        std::string                                  ScriptPath;
        std::unordered_map<std::string, ScriptParam> Params;

        // Visit serializes simple fields; Params is handled manually in EcsSerializer
        template<typename V>
        void Visit(V& v)
        {
            v("ScriptEnable", Enable);
            v("ScriptFile",   ScriptPath);
        }

        friend class ScriptSystem;

    private:
        lua_State* m_LuaState    = nullptr; // runtime-only
        int        m_InstanceRef = 0;       // runtime-only
    };
}

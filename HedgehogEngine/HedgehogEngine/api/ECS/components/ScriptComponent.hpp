#pragma once

#include <string>
#include <unordered_map>
#include <variant>

namespace HedgehogEngine
{
    enum class ParamType
    {
        Boolean,
        Number
    };

    struct ScriptParam
    {
        ParamType                 type;
        std::variant<bool, float> value;
    };

    // Which script an entity runs, whether it is enabled, and its parameter values. Data only:
    // the script system that runs it keeps all runtime state.
    struct ScriptComponent
    {
        bool                                         Enable = true;
        std::string                                  ScriptPath;
        std::unordered_map<std::string, ScriptParam> Params;

        // Visit serializes simple fields; Params is handled manually in EcsSerializer
        template<typename V>
        void Visit(V& v)
        {
            v("ScriptEnable", Enable);
            v("ScriptFile",   ScriptPath);
        }
    };
}

#pragma once

#include <optional>
#include <unordered_map>
#include <string>
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
        ParamType type;
        std::variant<bool, float> value;
        bool dirty = false;
    };

    // Which script an entity runs, and its settings. Pure data: the application's
    // script runtime turns it into a running instance in Play.
    struct ScriptComponent
    {
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
    };
}

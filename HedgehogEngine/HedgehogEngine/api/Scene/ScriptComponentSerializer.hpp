#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

namespace EcsSerialization
{
    class ComponentSerializerRegistry;
}

namespace HedgehogEngine
{
    // Registers ScriptComponent's YAML handler under "ScriptComponent". It writes ScriptEnable,
    // ScriptFile and, when there are any, the properties in order:
    //
    //   ScriptProperties:
    //     speed:  { Type: Number, Value: 7.45 }
    //     target: { Type: EntityRef, Value: 3 }                         # the entity's id
    //     model:  { Type: AssetRef, Value: Models/a.obj, AssetType: Mesh } # under assets://
    //
    // It reads that, or the legacy ScriptParams (ParamType 0 Boolean, 1 Number, value in
    // ParamValue). When a component has both, ScriptProperties wins and one warning is logged; a
    // property it cannot read is skipped with a warning.
    HEDGEHOG_ENGINE_API void RegisterScriptComponentSerializer(EcsSerialization::ComponentSerializerRegistry& registry);
}

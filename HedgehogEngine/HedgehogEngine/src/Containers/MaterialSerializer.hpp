#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include <string>

namespace HedgehogEngine
{
    struct MaterialData;

    class MaterialSerializer
    {
    public:
        HEDGEHOG_ENGINE_API static void Serialize(const MaterialData& material, std::string materialPath);
        HEDGEHOG_ENGINE_API static void Deserialize(MaterialData& material, std::string materialPath);
    };
}

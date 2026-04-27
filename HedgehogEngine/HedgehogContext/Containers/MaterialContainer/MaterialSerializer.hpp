#pragma once

#include "HedgehogContext/api/HedgehogContextApi.hpp"

#include <string>

namespace Context
{
    struct MaterialData;

    class MaterialSerializer
    {
    public:
        HEDGEHOG_CONTEXT_API static void Serialize(const MaterialData& material, std::string materialPath);

        HEDGEHOG_CONTEXT_API static void Deserialize(MaterialData& material, std::string materialPath);

    };

}



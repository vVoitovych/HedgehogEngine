#pragma once

#include "HedgehogEngine/api/Containers/MaterialData.hpp"

namespace HedgehogEngine
{
    struct MaterialDef
    {
        std::string  AlbedoPath;
        float        Transparency = 1.0f;
        MaterialType Type         = MaterialType::Opaque;
        bool         IsDirty      = false;
    };
}

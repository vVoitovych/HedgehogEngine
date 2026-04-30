#pragma once

#include "HedgehogEngine/api/Containers/MaterialData.hpp"

namespace HedgehogEngine
{
    struct MaterialDef
    {
        std::string  m_AlbedoPath;
        float        m_Transparency = 1.0f;
        MaterialType m_Type         = MaterialType::Opaque;
        bool         m_IsDirty      = false;
    };
}

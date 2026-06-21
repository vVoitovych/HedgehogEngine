#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include <optional>
#include <string>

namespace HedgehogEngine
{
HH_BEGIN_COMPONENT(MeshComponent)
    HH_PROP_NAMED(std::string, m_MeshPath, "MeshPath", std::string{}, None)

    std::optional<uint64_t> m_MeshIndex;      // runtime-only
    std::string             m_CachedMeshPath;  // runtime-only
HH_END_COMPONENT(MeshComponent)
}

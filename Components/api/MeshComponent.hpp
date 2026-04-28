#pragma once

#include <string>
#include <optional>

namespace Scene
{
    class MeshComponent
    {
    public:
        std::string             m_MeshPath;
        std::optional<uint64_t> m_MeshIndex;
        std::string             m_CachedMeshPath;
    };
}

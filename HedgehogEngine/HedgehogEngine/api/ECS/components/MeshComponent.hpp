#pragma once

#include <string>
#include <optional>

namespace HedgehogEngine
{
    class MeshComponent
    {
    public:
        std::string             m_MeshPath;
        std::optional<uint64_t> m_MeshIndex;      // runtime-only
        std::string             m_CachedMeshPath;  // runtime-only

        template<typename V>
        void Visit(V& v)
        {
            v("MeshPath", m_MeshPath);
        }
    };
}

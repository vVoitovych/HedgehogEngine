#pragma once 

#include "HedgehogMath/Vector.hpp"

#include <vector>

namespace ContentLoader
{

    struct LoadedVertexData
    {
        HM::Vector3 position;
        HM::Vector2 uv;
        HM::Vector3 normal;

        bool operator==(const LoadedVertexData& other) const {
            return position == other.position &&
                normal == other.normal &&
                uv == other.uv;
        }
    };

    struct LoadedMesh
    {
        std::vector<LoadedVertexData> vertices;
        std::vector<uint32_t> indices;
    };

}

namespace std
{
    template<>
    struct hash<ContentLoader::LoadedVertexData>
    {
        size_t operator()(ContentLoader::LoadedVertexData const& vertex) const
        {
            return (
                hash<HM::Vector3>()(vertex.position) ^
                (hash<HM::Vector2>()(vertex.uv) << 1) ^
                hash<HM::Vector3>()(vertex.normal));
        }
    };
}




#include "TangentStream.hpp"

#include <iterator>

namespace HR
{
    void AppendTangentStream(const HedgehogEngine::MeshView& mesh, std::vector<float>& tangents)
    {
        const size_t vertexCount = mesh.positions.size();
        if (mesh.tangents.size() != vertexCount)
        {
            for (size_t i = 0; i < vertexCount; ++i)
                tangents.insert(tangents.end(), std::begin(DEFAULT_TANGENT), std::end(DEFAULT_TANGENT));
            return;
        }

        for (const HM::Vector4& tangent : mesh.tangents)
        {
            tangents.push_back(tangent.x());
            tangents.push_back(tangent.y());
            tangents.push_back(tangent.z());
            tangents.push_back(tangent.w());
        }
    }
}

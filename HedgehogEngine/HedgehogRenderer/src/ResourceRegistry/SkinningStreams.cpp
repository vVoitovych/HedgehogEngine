#include "SkinningStreams.hpp"

namespace HR
{
    void AppendSkinningStreams(const HedgehogEngine::MeshView& mesh, std::vector<uint32_t>& joints,
                               std::vector<float>& weights)
    {
        const size_t vertexCount = mesh.positions.size();
        const bool   skinned     = mesh.joints.size() == vertexCount && mesh.weights.size() == vertexCount;
        if (!skinned)
        {
            joints.insert(joints.end(), vertexCount * 4, 0u);
            weights.insert(weights.end(), vertexCount * 4, 0.0f);
            return;
        }

        for (const HM::Vector4u& joint : mesh.joints)
        {
            joints.push_back(joint.x());
            joints.push_back(joint.y());
            joints.push_back(joint.z());
            joints.push_back(joint.w());
        }
        for (const HM::Vector4& weight : mesh.weights)
        {
            weights.push_back(weight.x());
            weights.push_back(weight.y());
            weights.push_back(weight.z());
            weights.push_back(weight.w());
        }
    }
}

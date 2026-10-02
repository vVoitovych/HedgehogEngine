#pragma once

#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Quaternion.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ContentLoader
{
    struct LoadedVertexData
    {
        HM::Vector3 position;
        HM::Vector2 uv;
        HM::Vector3 normal;

        bool operator==(const LoadedVertexData& other) const
        {
            return position == other.position &&
                   normal   == other.normal   &&
                   uv       == other.uv;
        }
    };

    // One joint of a skin. Joints are ordered parent before child, so Parent is always an
    // earlier index (or -1 for a root) and a pose can be built in one pass.
    struct LoadedJoint
    {
        std::string   Name;
        int32_t       Parent = -1;
        // Mesh space to the joint's space in the bind pose. Matrix4x4 stores columns, as glTF does.
        HM::Matrix4x4 InverseBindMatrix = HM::Matrix4x4::GetIdentity();
        // The joint node's local transform in the bind pose, relative to its parent joint's node.
        HM::Vector3    Translation = HM::Vector3(0.0f, 0.0f, 0.0f);
        HM::Quaternion Rotation    = HM::Quaternion::Identity();
        HM::Vector3    Scale       = HM::Vector3(1.0f, 1.0f, 1.0f);
    };

    struct LoadedSkin
    {
        std::vector<LoadedJoint> Joints;
    };

    struct LoadedMesh
    {
        std::vector<LoadedVertexData> vertices;
        std::vector<uint32_t>         indices;

        // Skinned meshes only; a static mesh leaves all three empty. When Skin is set, Joints and
        // Weights hold one entry per vertex: four indices into Skin->Joints and their weights,
        // which sum to 1.
        std::vector<HM::Vector4u>  Joints;
        std::vector<HM::Vector4>   Weights;
        std::optional<LoadedSkin>  Skin;
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

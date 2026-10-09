#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include "HedgehogAnimation/api/AnimationClip.hpp"
#include "HedgehogAnimation/api/Skeleton.hpp"

#include <optional>

#include <vector>
#include <string>

namespace HedgehogEngine
{
    class Mesh
    {
    public:
        // Returns false (after logging) when loading fails; the mesh is left empty.
        HEDGEHOG_ENGINE_API bool LoadData(const std::string& fileName,
                                          const FS::FileSystemManager& fileSystem);
        HEDGEHOG_ENGINE_API void ClearData();

        HEDGEHOG_ENGINE_API const std::vector<HM::Vector3>& GetPositions() const;
        HEDGEHOG_ENGINE_API const std::vector<HM::Vector2>& GetTexCoords() const;
        HEDGEHOG_ENGINE_API const std::vector<HM::Vector3>& GetNormals()   const;
        // One per vertex: xyz along increasing u, orthogonal to the normal; w the handedness.
        HEDGEHOG_ENGINE_API const std::vector<HM::Vector4>& GetTangents()  const;
        HEDGEHOG_ENGINE_API const std::vector<uint32_t>&    GetIndices()   const;

        // A skinned mesh's four joint indices (into its skeleton) and weights per vertex; empty
        // for a static mesh.
        HEDGEHOG_ENGINE_API const std::vector<HM::Vector4u>& GetJoints()  const;
        HEDGEHOG_ENGINE_API const std::vector<HM::Vector4>&  GetWeights() const;

        // A skinned mesh's skeleton, or nullptr for a static mesh, and the animation clips of
        // its file (none for a static mesh), loaded with the mesh.
        HEDGEHOG_ENGINE_API const HedgehogAnimation::Skeleton*                    GetSkeleton() const;
        HEDGEHOG_ENGINE_API const std::vector<HedgehogAnimation::AnimationClip>& GetAnimationClips() const;

        HEDGEHOG_ENGINE_API uint32_t GetIndexCount()   const;
        HEDGEHOG_ENGINE_API uint32_t GetFirstIndex()   const;
        HEDGEHOG_ENGINE_API uint32_t GetVertexOffset() const;

        HEDGEHOG_ENGINE_API void SetFirstIndex(uint32_t firstIndex);
        HEDGEHOG_ENGINE_API void SetVertexOffset(uint32_t offset);

    private:
        std::vector<HM::Vector3> m_Positions;
        std::vector<HM::Vector2> m_TexCoords;
        std::vector<HM::Vector3> m_Normals;
        std::vector<HM::Vector4> m_Tangents;

        std::vector<uint32_t> m_IndicesData;

        std::vector<HM::Vector4u> m_Joints;
        std::vector<HM::Vector4>  m_Weights;

        std::optional<HedgehogAnimation::Skeleton>     m_Skeleton;
        std::vector<HedgehogAnimation::AnimationClip> m_AnimationClips;

        uint32_t m_IndexCount    = 0;
        uint32_t m_FirstIndex    = 0;
        uint32_t m_VertexOffset  = 0;
    };
}

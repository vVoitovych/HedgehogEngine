#pragma once

#include "HedgehogContext/api/HedgehogContextApi.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <vector>
#include <string>

namespace Context
{
    class Mesh
    {
    public:
        HEDGEHOG_CONTEXT_API void LoadData(const std::string fileName);
        HEDGEHOG_CONTEXT_API void ClearData();

        HEDGEHOG_CONTEXT_API std::vector<HM::Vector3> GetPositions() const;
        HEDGEHOG_CONTEXT_API std::vector<HM::Vector2> GetTexCoords() const;
        HEDGEHOG_CONTEXT_API std::vector<HM::Vector3> GetNormals() const;
        HEDGEHOG_CONTEXT_API std::vector<uint32_t>    GetIndices() const;

        HEDGEHOG_CONTEXT_API uint32_t GetIndexCount() const;
        HEDGEHOG_CONTEXT_API uint32_t GetFirstIndex() const;
        HEDGEHOG_CONTEXT_API uint32_t GetVertexOffset() const;

        HEDGEHOG_CONTEXT_API void SetFirstIndex(uint32_t firstIndex);
        HEDGEHOG_CONTEXT_API void SetVertexOffset(uint32_t offset);

    private:
        std::vector<HM::Vector3> m_Positions;
        std::vector<HM::Vector2> m_TexCoords;
        std::vector<HM::Vector3> m_Normals;

        std::vector<uint32_t> m_IndicesData;

        uint32_t m_IndexCount;
        uint32_t m_FirstIndex;
        uint32_t m_VertexOffset;

    };
}



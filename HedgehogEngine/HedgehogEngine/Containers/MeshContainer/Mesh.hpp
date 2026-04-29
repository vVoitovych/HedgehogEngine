#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <vector>
#include <string>

namespace HedgehogEngine
{
    class Mesh
    {
    public:
        HEDGEHOG_ENGINE_API void LoadData(const std::string fileName);
        HEDGEHOG_ENGINE_API void ClearData();

        HEDGEHOG_ENGINE_API std::vector<HM::Vector3> GetPositions() const;
        HEDGEHOG_ENGINE_API std::vector<HM::Vector2> GetTexCoords() const;
        HEDGEHOG_ENGINE_API std::vector<HM::Vector3> GetNormals() const;
        HEDGEHOG_ENGINE_API std::vector<uint32_t>    GetIndices() const;

        HEDGEHOG_ENGINE_API uint32_t GetIndexCount() const;
        HEDGEHOG_ENGINE_API uint32_t GetFirstIndex() const;
        HEDGEHOG_ENGINE_API uint32_t GetVertexOffset() const;

        HEDGEHOG_ENGINE_API void SetFirstIndex(uint32_t firstIndex);
        HEDGEHOG_ENGINE_API void SetVertexOffset(uint32_t offset);

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



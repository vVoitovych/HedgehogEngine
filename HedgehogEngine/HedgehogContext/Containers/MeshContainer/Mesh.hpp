#pragma once

#include "HedgehogMath/Vector.hpp"

#include <vector>
#include <string>

namespace Context
{
	class Mesh
	{
	public:
		void LoadData(const std::string fileName);
		void ClearData();

		std::vector<HM::Vector3> GetPositions() const;
		std::vector<HM::Vector2> GetTexCoords() const;
		std::vector<HM::Vector3> GetNormals() const;
		std::vector<uint32_t> GetIndicies() const;

		uint32_t GetIndexCount() const;
		uint32_t GetFirstIndex() const;
		uint32_t GetVertexOffset() const;

		void SetFirstIndex(uint32_t firstIndex);
		void SetVertexOffset(uint32_t offset);

	private:
		std::vector<HM::Vector3> m_Positions;
		std::vector<HM::Vector2> m_TexCoords;
		std::vector<HM::Vector3> m_Normals;

		std::vector<uint32_t> m_IndiciesData;

		uint32_t m_IndexCount;
		uint32_t m_FirstIndex;
		uint32_t m_VertexOffset;

	};
}



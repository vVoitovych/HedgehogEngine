#pragma once
#include "VertexDescription.hpp"

#include <vector>
#include <string>

namespace Renderer
{
	class Mesh
	{
	public:
		void LoadData(const std::string fileName);
		void ClearData();

		std::vector<VertexDescription> GetVerticies() const;
		std::vector<uint32_t> GetIndicies() const;

		uint32_t GetIndexCount() const;
		uint32_t GetFirstIndex() const;
		uint32_t GetVertexOffset() const;

		void SetFirstIndex(uint32_t firstIndex);
		void SetVertexOffset(uint32_t offset);

	private:
		std::vector<VertexDescription> mVerticiesData;
		std::vector<uint32_t> mIndiciesData;

		uint32_t mIndexCount;
		uint32_t mFirstIndex;
		uint32_t mVertexOffset;

	};
}



#pragma once 

#include "HedgehogMath/Vector.hpp"

#include <vector>

namespace ContentLoader
{

	struct LoadedVertexData
	{
		HM::Vector4 position;
		HM::Vector2 uv;
		HM::Vector4 normal;

		HM::Vector4u jointIndex;
		HM::Vector4 jointWeight;
	};

	struct LoadedMesh
	{
		std::vector<LoadedVertexData> verticies;
		std::vector<uint32_t> indicies;
	};

}






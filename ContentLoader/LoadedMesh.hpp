#pragma once 

#include "HedgehogMath/Vector.hpp"

#include <vector>

namespace ContentLoader
{
	struct LoadedMesh
	{
		std::vector<HM::Vector3> mPositions;
		std::vector<HM::Vector2> mTexCoords;
		std::vector<HM::Vector3> mNormals;

		std::vector<uint32_t> mIndicies;

		std::vector<HM::Vector4u> mJointIndices; 
		std::vector<HM::Vector4> mJointWeights;

	};

}






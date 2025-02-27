#pragma once 

#include "HedgehogMath/Vector.hpp"

#include <vector>

namespace ContentLoader
{

	struct LoadedVertexData
	{
		HM::Vector3 position;
		HM::Vector2 uv;
		HM::Vector3 normal;
		HM::Vector3 tangent;  
		HM::Vector3 bitangent; 

		bool operator==(const LoadedVertexData& other) const {
			return position == other.position &&
				normal == other.normal &&
				uv == other.uv;
		}
	};

	struct LoadedMesh
	{
		std::vector<LoadedVertexData> verticies;
		std::vector<uint32_t> indicies;
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




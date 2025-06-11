#pragma once

#include <string>
#include <optional>

namespace Scene
{
	class MeshComponent
	{
	public:
		std::string mMeshPath;
		std::optional<uint64_t> mMeshIndex;

		std::string mCachedMeshPath;

	};
}


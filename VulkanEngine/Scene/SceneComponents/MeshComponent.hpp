#pragma once

#include <string>
#include <optional>

namespace Scene
{
	class MeshComponent
	{
	public:
		std::string mMeshPath;
		std::optional<size_t> mMeshIndex;

		std::string mCachedMeshPath;

	};
}


#pragma once

#include <string>

namespace Renderer
{
	enum class MaterialType
	{
		Opaque,
		Cutoff,
		Transparency
	};

	struct MaterialData
	{
		std::string path;

		MaterialType type;
		std::string baseColor;
		float tansparency;
	};

}





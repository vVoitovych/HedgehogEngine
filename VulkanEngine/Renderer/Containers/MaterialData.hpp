#pragma once

#include <string>

namespace Renderer
{
	enum class MaterialType
	{
		Opaque,
		Cutoff,
		Transparent
	};

	struct MaterialData
	{
		std::string path;

		MaterialType type;
		std::string baseColor;
		float transparency;

		bool isDirty;
	};

}





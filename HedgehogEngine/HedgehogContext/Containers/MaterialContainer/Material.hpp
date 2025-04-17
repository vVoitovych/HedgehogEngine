#pragma once

#include <string>

namespace Context
{
	enum class MaterialSurfaceType
	{
		Opaque,
		Cutoff,
		Transparent
	};

	enum class CullingType
	{
		None, 
		Back, 
		Front
	};

	struct Material
	{
		std::string path;

		std::string vertexShader;
		std::string fragmentShader;

		std::string tessellationShader;
		std::string geometryShader;

		MaterialSurfaceType materialType;
		CullingType cullingType;
	};

}



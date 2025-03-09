#pragma once

#include <string>

namespace ContentLoader
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

	class MaterialSerializer
	{
	public:
		static void Serialize(MaterialData& material, std::string materialPath);

		static void Deserialize(MaterialData& material, std::string materialPath);

	};

}



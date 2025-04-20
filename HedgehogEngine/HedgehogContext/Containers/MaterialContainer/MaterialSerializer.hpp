#pragma once

#include <string>

namespace Context
{
	struct MaterialData;
	class Material;

	class MaterialSerializer
	{
	public:
		static void Serialize(MaterialData& material, std::string materialPath);

		static void Deserialize(MaterialData& material, std::string materialPath);

		static void Serialize(Material& material, std::string materialPath);

		static void Deserialize(Material& material, std::string materialPath);
	};

}



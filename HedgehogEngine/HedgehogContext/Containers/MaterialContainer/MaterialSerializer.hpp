#pragma once

#include <string>

namespace Context
{
	struct MaterialData;
	class Material;
	class MaterialInstance;

	class MaterialSerializer
	{
	public:
		[[deprecated]]
		static void Serialize(MaterialData& material, std::string materialPath);

		[[deprecated]]
		static void Deserialize(MaterialData& material, std::string materialPath);

		static void Serialize(Material& material, std::string materialPath);

		static void Deserialize(Material& material, std::string materialPath);

		static void Serialize(MaterialInstance& materialInstance, std::string materialInstancePath);

		static void Deserialize(MaterialInstance& materialInstance, std::string materialInstancePath);
	};

}



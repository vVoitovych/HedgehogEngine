#pragma once

#include <string>

namespace Context
{
	struct MaterialData;

	class MaterialSerializer
	{
	public:
		static void Serialize(MaterialData& material, std::string materialPath);

		static void Deserialize(MaterialData& material, std::string materialPath);

	};

}



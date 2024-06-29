#pragma once

#include <string>

namespace Renderer
{
	struct MaterialData;

	class MaterialSerializer
	{
	public:
		static void Serialize(MaterialData& material, std::string materialPath);

		static void Deserialize(MaterialData& material, std::string materialPath);

	};

}



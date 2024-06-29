#pragma once

#include <string>
#include <optional>

namespace Scene
{

	class RenderComponent
	{
	public:
		bool mIsVisible = true;

		std::string mMaterial;
		std::optional<size_t> mMaterialIndex;

	};
}




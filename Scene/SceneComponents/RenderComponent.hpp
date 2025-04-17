#pragma once

#include <string>
#include <optional>

namespace Scene
{

	class RenderComponent
	{
	public:
		bool mIsVisible = true;

		std::string mMaterialInstance;
		std::optional<size_t> mMaterialIndex;

	};
}




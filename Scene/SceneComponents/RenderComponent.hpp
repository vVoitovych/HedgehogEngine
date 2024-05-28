#pragma once

#include <string>
#include <optional>

namespace Scene
{
	class RenderComponent
	{
	public:
		bool mIsVisible;

		std::string mMaterial;
		bool mDirty = false;
	};
}




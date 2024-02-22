#pragma once

#include <vector>

namespace Renderer
{
	struct Light;

	class LightContainer
	{
	public:
		LightContainer();

	private:
		std::vector<Light> mLights;
	};

}


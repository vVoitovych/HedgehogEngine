#pragma once

#include <vector>
#include <memory>

namespace Renderer
{
	struct Light;

	class RenderContext;

	class LightContainer
	{
	public:
		LightContainer();
		void UpdateLights(const std::unique_ptr<RenderContext>& context);

	private:
		std::vector<Light> mLights;

		size_t mCachedLightComponentCount;
	};

}


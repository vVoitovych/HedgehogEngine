#pragma once

#include "Light.hpp"

#include <vector>
#include <memory>

namespace Scene
{
	class Scene;
}

namespace Renderer
{
	class LightContainer
	{
	public:
		LightContainer();
		void UpdateLights(const Scene::Scene& scene);
		size_t GetLightCount() const;
		const std::vector<Light>& GetLights() const;
	private:
		std::vector<Light> mLights;

		size_t mCachedLightComponentCount;
		size_t mLightCont;
	};

}


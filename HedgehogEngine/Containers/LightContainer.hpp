#pragma once

#include "Light.hpp"

#include <vector>

namespace Scene
{
	class Scene;
}

namespace Context
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


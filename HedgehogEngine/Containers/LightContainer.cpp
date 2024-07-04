#include "LightContainer.hpp"

#include "Scene/Scene.hpp"
#include "Scene/SceneComponents/LightComponent.hpp"

#include "Common/RendererSettings.hpp"
#include "Common/EngineDebugBreak.hpp"
#include "Logger/Logger.hpp"

namespace Context
{
	LightContainer::LightContainer()
	{
		mLights.resize(MAX_LIGHTS_COUNT);
	}

	void LightContainer::UpdateLights(const Scene::Scene& scene)
	{
		auto lightComponentsCount = scene.GetLightCount();
		if (lightComponentsCount > MAX_LIGHTS_COUNT)
		{
			LOGWARNING("To many light components. Some of them won't be processed!");
		}
		lightComponentsCount = std::min(lightComponentsCount, static_cast<size_t>(MAX_LIGHTS_COUNT));
		size_t counter = 0;
		for (size_t i = 0; i < lightComponentsCount; ++i)
		{
			const auto& lightComponent = scene.GetLightComponentByIndex(i);
			if (lightComponent.mEnable)
			{
				auto& light = mLights[counter];
				light.mPosition = lightComponent.mPosition;
				light.mDirection = lightComponent.mDirection;
				light.mColor = lightComponent.mColor;
				light.mData = { 
					lightComponent .mLightType, 
					lightComponent.mIntencity, 
					lightComponent.mRadius, 
					cos(glm::radians(lightComponent.mConeAngle))};
				++counter;
			}
		}
		mLightCont = counter;
	}

	size_t LightContainer::GetLightCount() const
	{
		return mLightCont;
	}

	const std::vector<Light>& LightContainer::GetLights() const
	{
		return mLights;
	}


}

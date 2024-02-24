#include "LightContainer.hpp"
#include "Light.hpp"

#include "VulkanEngine/Renderer/Common/RendererSettings.hpp"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.hpp"
#include "Logger/Logger.hpp"

namespace Renderer
{
	LightContainer::LightContainer()
	{
		mLights.resize(MAX_LIGHTS_COUNT);
	}

	void LightContainer::UpdateLights(const std::unique_ptr<RenderContext>& context)
	{
	}


}

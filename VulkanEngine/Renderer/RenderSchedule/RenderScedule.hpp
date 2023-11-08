#pragma once

#include "VulkanEngine/Renderer/RenderPasses/ForwardPass.hpp"
#include "VulkanEngine/Renderer/RenderPasses/PresentPass.hpp"

namespace Renderer
{
	class Device;
	class RenderContext;
	class ResourceTracker;

	class RenderScedule
	{
	public:
		virtual void Render(RenderContext& renderContext, ResourceTracker& resourceTracker);
		virtual void Initialize(Device& device, RenderContext& renderContext);
		virtual void Cleanup(Device& device);

	private:
		ForwardPass mForwardPass;
		PresentPass mPresentPass;

	};
}


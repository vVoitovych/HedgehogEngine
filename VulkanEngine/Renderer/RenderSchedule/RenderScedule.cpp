#include "RenderScedule.hpp"

#include <cstdint>

namespace Renderer
{
	void RenderScedule::Render(RenderContext& renderContext, ResourceTracker& resourceTracker)
	{
		mForwardPass.Render(renderContext, resourceTracker);
		mPresentPass.Render(renderContext, resourceTracker);
	}

	void RenderScedule::Initialize(Device& device, RenderContext& renderContext)
	{
		mForwardPass.Initialize(device, renderContext);
		mPresentPass.Initialize(device, renderContext);
	}

	void RenderScedule::Cleanup(Device& device)
	{
		mForwardPass.Cleanup(device);
		mPresentPass.Cleanup(device);
	}
}



#pragma once

#include "BaseRenderPass.hpp"
#include "VulkanEngine/Renderer/Wrappeers/RenderPass/RenderPass.hpp"
#include "VulkanEngine/Renderer/Wrappeers/FrameBuffer/FrameBuffer.hpp"

namespace Renderer
{
    class Device;
    class RenderContext;
    class ResourceTracker;

	class ForwardPass : public BaseRenderPass
	{
    public:
        ForwardPass() = default;
        virtual ~ForwardPass() = default;

        virtual void Render(RenderContext& renderContext, ResourceTracker& resourceTracker);
        virtual void Initialize(Device & device, RenderContext & renderContext);
        virtual void Cleanup(Device & device);

    private:
        RenderPass mRenderPass;
        FrameBuffer mFrameBuffer;

	};

}



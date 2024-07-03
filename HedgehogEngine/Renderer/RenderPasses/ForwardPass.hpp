#pragma once

#include <memory>
#include <string>

namespace Renderer
{
    class RenderContext;
    class ResourceManager;
    class RenderPass;
    class DescriptorSetLayout;
    class DescriptorAllocator;
    class Pipeline;
    class FrameBuffer;
    class DescriptorSet;

	class ForwardPass
	{
    public:
        ForwardPass(const RenderContext& context, const ResourceManager& resourceManager);
        ~ForwardPass();

        void Render(RenderContext& context, const ResourceManager& resourceManager);
        void Cleanup(const RenderContext& context);

        void ResizeResources(const RenderContext& context, const ResourceManager& resourceManager);

    private:
        std::unique_ptr<RenderPass> mRenderPass;
        std::unique_ptr<FrameBuffer> mFrameBuffer;
        std::unique_ptr<Pipeline> mPipeline;
             
	};

}



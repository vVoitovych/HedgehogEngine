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
        ForwardPass(const std::unique_ptr<RenderContext>& context, const std::unique_ptr<ResourceManager>& resourceManager);
        ~ForwardPass();

        void Render(std::unique_ptr<RenderContext>& context, const std::unique_ptr<ResourceManager>& resourceManager);
        void Cleanup(const std::unique_ptr<RenderContext>& context);

        void ResizeResources(const std::unique_ptr<RenderContext>& context, const std::unique_ptr<ResourceManager>& resourceManager);

    private:
        std::unique_ptr<RenderPass> mRenderPass;
        std::unique_ptr<FrameBuffer> mFrameBuffer;
        std::unique_ptr<DescriptorSetLayout> mDescriptorSetLayout;
        std::unique_ptr<DescriptorAllocator> mDescriptorAllocator;
        std::unique_ptr<Pipeline> mPipeline;

        std::unique_ptr<DescriptorSet> mDescriptorSet;
        

	};

}



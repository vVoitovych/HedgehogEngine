#pragma once

#include <memory>
#include <vector>
#include <string>

namespace Renderer
{
    class RenderContext;
    class ResourceManager;
    class RenderPass;
    class DescriptorSetLayout;
    class Pipeline;
    class FrameBuffer;
    class DescriptorSet;
    class UBO;
    class Image;

	class ForwardPass
	{
    public:
        ForwardPass(const std::unique_ptr<RenderContext>& context, const std::unique_ptr<ResourceManager>& resourceManager);
        ~ForwardPass();

        void Render(std::unique_ptr<RenderContext>& context);
        void Cleanup(const std::unique_ptr<RenderContext>& context);

        void ResizeResources(const std::unique_ptr<RenderContext>& context, const std::unique_ptr<ResourceManager>& resourceManager);

    private:
        std::unique_ptr<RenderPass> mRenderPass;
        std::unique_ptr<DescriptorSetLayout> mDescriptorSetLayout;
        std::unique_ptr<Pipeline> mPipeline;
        std::unique_ptr<FrameBuffer> mFrameBuffer;

        std::vector<DescriptorSet> mDescriptorSets;
        std::vector<UBO> mUniformBuffers;

	};

}



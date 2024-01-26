#pragma once

#include <memory>
#include <vector>
#include <string>

namespace Renderer
{
    class RenderContext;

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
        ForwardPass(const std::unique_ptr<RenderContext>& context);
        ~ForwardPass();

        void Render(std::unique_ptr<RenderContext>& context);
        void Cleanup(const std::unique_ptr<RenderContext>& context);

        void CleanSizedResources(const std::unique_ptr<RenderContext>& context);
        void CreateSizedResources(const std::unique_ptr<RenderContext>& context);

    private:
        void CreateDepthBuffer(const std::unique_ptr<RenderContext>& context);

    private:
        std::unique_ptr<RenderPass> mRenderPass;
        std::unique_ptr<DescriptorSetLayout> mDescriptorSetLayout;
        std::unique_ptr<Pipeline> mPipeline;

        std::unique_ptr<Image> mDepthBuffer;
        std::vector<FrameBuffer> mFrameBuffers;

        std::vector<DescriptorSet> mDescriptorSets;
        std::vector<UBO> mUniformBuffers;

	};

}



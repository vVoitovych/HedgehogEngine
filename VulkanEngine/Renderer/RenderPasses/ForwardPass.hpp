#pragma once

#include <memory>
#include <vector>

namespace Renderer
{
    class RenderContext;
    class Device;
    class SwapChain;

    class RenderPass;
    class DescriptorSetLayout;
    class Pipeline;
    class DepthBuffer;
    class FrameBuffer;
    class DescriptorSet;
    class UBO;
    class TextureImage;
    class TextureSampler;

	class ForwardPass
	{
    public:
        ForwardPass(const std::unique_ptr<Device>& device, const std::unique_ptr<SwapChain>& swapChain);
        ~ForwardPass();

        void Render(std::unique_ptr<RenderContext>& renderContext);
        void Cleanup(const std::unique_ptr<Device>& device);

        void CleanSizedResources(const std::unique_ptr<Device>& device);
        void CreateSizedResources(const std::unique_ptr<Device>& device, const std::unique_ptr<SwapChain>& swapChain);

    private:
        std::unique_ptr<RenderPass> mRenderPass;
        std::unique_ptr<DescriptorSetLayout> mDescriptorSetLayout;
        std::unique_ptr<Pipeline> mPipeline;

        std::unique_ptr<DepthBuffer> mDepthBuffer;
        std::vector<FrameBuffer> mFrameBuffers;

        std::unique_ptr<DescriptorSet> mDescriptorSet;
        std::unique_ptr<UBO> mUniformBuffer;

        // TODO remome teture and texture sampler from render pass
        std::unique_ptr<TextureImage> mTextureImage;
        std::unique_ptr<TextureSampler> mTextureSampler;
	};

}



#include "ForwardPass.hpp"
#include "ForwardPassInfo.hpp"
#include "ForwardPipelineInfo.hpp"

#include "VulkanEngine/Renderer/Context/RenderContext.hpp"
#include "VulkanEngine/Renderer/Context/EngineContext.hpp"
#include "VulkanEngine/Renderer/Context/ThreadContext.hpp"

#include "VulkanEngine/Renderer/Wrappeers/Device/Device.hpp"
#include "VulkanEngine/Renderer/Wrappeers/SwapChain/SwapChain.hpp"
#include "VulkanEngine/Renderer/Wrappeers/RenderPass/RenderPass.hpp"

#include "VulkanEngine/Renderer/Wrappeers/Resources/DepthBuffer/DepthBuffer.hpp"
#include "VulkanEngine/Renderer/Wrappeers/FrameBuffer/FrameBuffer.hpp"
#include "VulkanEngine/Renderer/Wrappeers/RenderPass/RenderPass.hpp"
#include "VulkanEngine/Renderer/Wrappeers/Descriptors/DescriptorSetLayout.hpp"
#include "VulkanEngine/Renderer/Wrappeers/Pipeline/Pipeline.hpp"
#include "VulkanEngine/Renderer/Wrappeers/Descriptors/DescriptorSet.hpp"
#include "VulkanEngine/Renderer/Wrappeers/Descriptors/UBO.hpp"
#include "VulkanEngine/Renderer/Common/RendererSettings.hpp"
// TODO remome texture and texture sampler from render pass
#include "VulkanEngine/Renderer/Wrappeers/Resources/TextureImage/TextureImage.hpp"
#include "VulkanEngine/Renderer/Wrappeers/Resources/TextureImage/TextureSampler.hpp"

namespace Renderer
{
	void ForwardPass::Render(std::unique_ptr<RenderContext>& renderContext)
	{
		auto [engineContext, frameContext, threadContext] = renderContext->GetContexts();

		auto& commandBuffer = threadContext->GetCommandBuffer();
		auto frameIndex = threadContext->GetFrame();
		auto extend = engineContext->GetExtent();
		auto backBufferIndex = engineContext->GetBackBufferIndex();

		mUniformBuffers[frameIndex].UpdateUniformBuffer(renderContext);
		
		commandBuffer.BeginRenderPass(extend, mRenderPass, mFrameBuffers[backBufferIndex].GetNativeFrameBuffer());
		commandBuffer.BindPipeline(mPipeline, VK_PIPELINE_BIND_POINT_GRAPHICS);
		commandBuffer.SetViewport(0.0f, 0.0f, (float)extend.width, (float)extend.height, 0.0f, 1.0f);
		commandBuffer.SetScissor({ 0, 0 }, extend);
		auto& meshContainer = engineContext->GetMeshContainer();
		VkBuffer vertexBuffers[] = { meshContainer.GetVertexBuffer() };
		VkDeviceSize offsets[] = { 0 };
		commandBuffer.BindVertexBuffers(0, 1, vertexBuffers, offsets);
		commandBuffer.BindIndexBuffer(meshContainer.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
		commandBuffer.BindDescriptorSers(VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline, 0, 1, mDescriptorSets[frameIndex].GetNativeSet(), 0, nullptr);

		auto& mesh = meshContainer.GetMesh(0);
		commandBuffer.DrawIndexed(mesh.GetIndexCount(), 1, mesh.GetFirstIndex(), mesh.GetVertexOffset(), 0);
		commandBuffer.EndRenderPass();
		
	}

	ForwardPass::ForwardPass(const std::unique_ptr<Device>& device, const std::unique_ptr<SwapChain>& swapChain)
	{
		ForwardPassInfo info{ swapChain->GetFormat(), device->FindDepthFormat() };
		mRenderPass = std::make_unique<RenderPass>(device, info.GetInfo());
		mDescriptorSetLayout = std::make_unique<DescriptorSetLayout>(device);
		std::unique_ptr<PipelineInfo> pipelineInfo = std::make_unique<ForwardPipelineInfo>(device);
		mPipeline = std::make_unique<Pipeline>(device, mRenderPass, mDescriptorSetLayout, pipelineInfo);

		mTextureImage = std::make_unique<TextureImage>(device, "Textures\\viking_room.png", VK_FORMAT_R8G8B8A8_SRGB);
		mTextureSampler = std::make_unique<TextureSampler>(device);

		mDepthBuffer = std::make_unique<DepthBuffer>(device, swapChain->GetSwapChainExtend());

		mFrameBuffers.clear();
		mUniformBuffers.clear();
		mDescriptorSets.clear();
		size_t swapChainImagesSize = swapChain->GetSwapChainImagesSize();
		for (size_t i = 0; i < swapChainImagesSize; ++i)
		{
			std::vector<VkImageView> attacments = { swapChain->GetNativeSwapChainImageView(i), mDepthBuffer->GetNativeView() };
			FrameBuffer frameBuffer(
				device,
				attacments,
				swapChain->GetSwapChainExtend(),
				mRenderPass);
			mFrameBuffers.push_back(std::move(frameBuffer));
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			UBO uniformBuffer(device);
			mUniformBuffers.push_back(std::move(uniformBuffer));

			DescriptorSet descriptorSet(
				device, 
				mDescriptorSetLayout, 
				mUniformBuffers[i], 
				mTextureImage, 
				mTextureSampler);
			mDescriptorSets.push_back(std::move(descriptorSet));
		}

		pipelineInfo->Cleanup(device);
	}

	ForwardPass::~ForwardPass()
	{
	}

	void ForwardPass::Cleanup(const std::unique_ptr<Device>& device)
	{
		for (size_t i = 0; i < mFrameBuffers.size(); ++i)
		{
			mFrameBuffers[i].Cleanup(device);
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			mDescriptorSets[i].Cleanup(device);
			mUniformBuffers[i].Cleanup(device);
		}
		mDepthBuffer->Cleanup(device);
	
		mTextureImage->Cleanup(device);
		mTextureSampler->Cleanup(device);

		mPipeline->Cleanup(device);
		mDescriptorSetLayout->Cleanup(device);
		mRenderPass->Cleanup(device);

	}

	void ForwardPass::CleanSizedResources(const std::unique_ptr<Device>& device)
	{
		for (size_t i = 0; i < mFrameBuffers.size(); ++i)
		{
			mFrameBuffers[i].Cleanup(device);
		}
		mDepthBuffer->Cleanup(device);
	}

	void ForwardPass::CreateSizedResources(const std::unique_ptr<Device>& device, const std::unique_ptr<SwapChain>& swapChain)
	{
		mDepthBuffer = std::make_unique<DepthBuffer>(device, swapChain->GetSwapChainExtend());
		mFrameBuffers.clear();
		size_t swapChainImagesSize = swapChain->GetSwapChainImagesSize();
		for (size_t i = 0; i < swapChainImagesSize; ++i)
		{
			mFrameBuffers.push_back(FrameBuffer(
				device,
				{ swapChain->GetNativeSwapChainImageView(i), mDepthBuffer->GetNativeView() },
				swapChain->GetSwapChainExtend(),
				mRenderPass));
		}
	}


}


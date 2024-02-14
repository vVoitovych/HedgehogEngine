#include "ForwardPass.hpp"
#include "ForwardPassInfo.hpp"
#include "ForwardPipelineInfo.hpp"

#include "Renderer/Context/RenderContext.hpp"
#include "Renderer/Context/EngineContext.hpp"
#include "Renderer/Context/ThreadContext.hpp"
#include "Renderer/Context/VulkanContext.hpp"
#include "Renderer/Context/FrameContext.hpp"

#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/SwapChain/SwapChain.hpp"
#include "Renderer/Wrappeers/RenderPass/RenderPass.hpp"
#include "Renderer/Wrappeers/Commands/CommandBuffer.hpp"
#include "Renderer/Wrappeers/FrameBuffer/FrameBuffer.hpp"
#include "Renderer/Wrappeers/RenderPass/RenderPass.hpp"
#include "Renderer/Wrappeers/Descriptors/DescriptorSetLayout.hpp"
#include "Renderer/Wrappeers/Pipeline/Pipeline.hpp"
#include "Renderer/Wrappeers/Descriptors/DescriptorSet.hpp"
#include "Renderer/Wrappeers/Descriptors/UBO.hpp"
#include "Renderer/Wrappeers/Resources/Image/Image.hpp"

#include "Renderer/Containers/MeshContainer.hpp"
#include "Renderer/Containers/TextureContainer.hpp"
#include "Renderer/Containers/SamplerContainer.h"

#include "Renderer/Common/RendererSettings.hpp"
#include "Logger/Logger.hpp"

namespace Renderer
{
	void ForwardPass::Render(std::unique_ptr<RenderContext>& context)
	{
		auto& frameContext = context->GetFrameContext();
		auto& threadContext = context->GetThreadContext();
		auto& vulkanContext = context->GetVulkanContext();
		auto& engineContext = context->GetEngineContext();

		auto& commandBuffer = threadContext->GetCommandBuffer();
		auto frameIndex = threadContext->GetFrame();
		auto extend = vulkanContext->GetSwapChain()->GetSwapChainExtent();
		auto backBufferIndex = frameContext->GetBackBufferIndex();

		mUniformBuffers[frameIndex].UpdateUniformBuffer(context);
		
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

		for (auto& meshData : engineContext->GetScene().GetMeshEntities())
		{
			auto& mesh = meshContainer.GetMesh(meshData.second);
			commandBuffer.DrawIndexed(mesh.GetIndexCount(), 1, mesh.GetFirstIndex(), mesh.GetVertexOffset(), 0);
		}
		commandBuffer.EndRenderPass();
		
	}

	ForwardPass::ForwardPass(const std::unique_ptr<RenderContext>& context)
	{
		auto& vulkanContext = context->GetVulkanContext();

		ForwardPassInfo info{ vulkanContext->GetSwapChain()->GetFormat(), vulkanContext->GetDevice()->FindDepthFormat() };
		mRenderPass = std::make_unique<RenderPass>(vulkanContext->GetDevice(), info.GetInfo());
		mDescriptorSetLayout = std::make_unique<DescriptorSetLayout>(vulkanContext->GetDevice());
		std::unique_ptr<PipelineInfo> pipelineInfo = std::make_unique<ForwardPipelineInfo>(vulkanContext->GetDevice());
		mPipeline = std::make_unique<Pipeline>(vulkanContext->GetDevice(), mRenderPass, mDescriptorSetLayout, pipelineInfo);

		CreateDepthBuffer(context);		

		mFrameBuffers.clear();
		mUniformBuffers.clear();
		mDescriptorSets.clear();
		size_t swapChainImagesSize = vulkanContext->GetSwapChain()->GetSwapChainImagesSize();
		for (size_t i = 0; i < swapChainImagesSize; ++i)
		{
			std::vector<VkImageView> attacments = { vulkanContext->GetSwapChain()->GetNativeSwapChainImageView(i), mDepthBuffer->GetNativeView() };
			FrameBuffer frameBuffer(
				vulkanContext->GetDevice(),
				attacments,
				vulkanContext->GetSwapChain()->GetSwapChainExtent(),
				mRenderPass);
			mFrameBuffers.push_back(std::move(frameBuffer));
		}
		auto& engineContext = context->GetEngineContext();
		auto& materialImage = engineContext->GetTextureContainer().GetImage(0);
		auto& materalSampler = engineContext->GetSamplerContainer().GetSampler(SamplerType::Linear);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			UBO uniformBuffer(vulkanContext->GetDevice());
			mUniformBuffers.push_back(std::move(uniformBuffer));

			DescriptorSet descriptorSet(
				vulkanContext->GetDevice(),
				vulkanContext->GetDescriptorPool(),
				mDescriptorSetLayout, 
				mUniformBuffers[i], 
				materialImage,
				materalSampler);
			mDescriptorSets.push_back(std::move(descriptorSet));
		}

		pipelineInfo->Cleanup(vulkanContext->GetDevice());
	}

	ForwardPass::~ForwardPass()
	{
	}

	void ForwardPass::Cleanup(const std::unique_ptr<RenderContext>& context)
	{
		auto& vulkanContext = context->GetVulkanContext();

		for (size_t i = 0; i < mFrameBuffers.size(); ++i)
		{
			mFrameBuffers[i].Cleanup(vulkanContext->GetDevice());
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			mDescriptorSets[i].Cleanup(vulkanContext->GetDevice(), vulkanContext->GetDescriptorPool());
			mUniformBuffers[i].Cleanup();
		}
		mDepthBuffer->Cleanup();

		mPipeline->Cleanup(vulkanContext->GetDevice());
		mDescriptorSetLayout->Cleanup(vulkanContext->GetDevice());
		mRenderPass->Cleanup(vulkanContext->GetDevice());

	}

	void ForwardPass::ResizeResources(const std::unique_ptr<RenderContext>& context)
	{
		auto& vulkanContext = context->GetVulkanContext();
		for (size_t i = 0; i < mFrameBuffers.size(); ++i)
		{
			mFrameBuffers[i].Cleanup(vulkanContext->GetDevice());
		}
		mDepthBuffer->Cleanup();

		CreateDepthBuffer(context);
		mFrameBuffers.clear();

		size_t swapChainImagesSize = vulkanContext->GetSwapChain()->GetSwapChainImagesSize();
		for (size_t i = 0; i < swapChainImagesSize; ++i)
		{
			mFrameBuffers.push_back(FrameBuffer(
				vulkanContext->GetDevice(),
				{ vulkanContext->GetSwapChain()->GetNativeSwapChainImageView(i), mDepthBuffer->GetNativeView() },
				vulkanContext->GetSwapChain()->GetSwapChainExtent(),
				mRenderPass));
		}
	}

	void ForwardPass::CreateDepthBuffer(const std::unique_ptr<RenderContext>& context)
	{
		auto& vulkanContext = context->GetVulkanContext();
		auto depthFormat = vulkanContext->GetDevice()->FindDepthFormat();
		auto extend = vulkanContext->GetSwapChain()->GetSwapChainExtent();

		mDepthBuffer = std::make_unique<Image>(
			vulkanContext->GetDevice(),
			extend.width, 
			extend.height, 
			depthFormat, 
			VK_IMAGE_TILING_OPTIMAL, 
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
		mDepthBuffer->CreateImageView(depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
		LOGINFO("Depth buffer created");

	}


}


#include "ForwardPass.hpp"
#include "ForwardPassInfo.hpp"
#include "ForwardPipelineInfo.hpp"
#include "ForwardPassPushConstants.hpp"

#include "Renderer/Context/RenderContext.hpp"
#include "Renderer/Context/EngineContext.hpp"
#include "Renderer/Context/ThreadContext.hpp"
#include "Renderer/Context/VulkanContext.hpp"
#include "Renderer/Context/FrameContext.hpp"

#include "Renderer/ResourceManager/ResourceManager.hpp"

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
#include "Scene/RenderObjectsManager.hpp"
#include "Renderer/Common/RendererSettings.hpp"
#include "Logger/Logger.hpp"

namespace Renderer
{
	void ForwardPass::Render(std::unique_ptr<RenderContext>& context, const std::unique_ptr<ResourceManager>& resourceManager)
	{
		auto& frameContext = context->GetFrameContext();
		auto& threadContext = context->GetThreadContext();
		auto& vulkanContext = context->GetVulkanContext();
		auto& engineContext = context->GetEngineContext();

		auto& commandBuffer = threadContext->GetCommandBuffer();
		auto frameIndex = threadContext->GetFrame();
		auto extend = vulkanContext->GetSwapChain().GetSwapChainExtent();
		auto backBufferIndex = frameContext->GetBackBufferIndex();

		mUniformBuffers[frameIndex].UpdateUniformBuffer(*context);
		
		commandBuffer.BeginRenderPass(extend, *mRenderPass, mFrameBuffer->GetNativeFrameBuffer());
		commandBuffer.BindPipeline(*mPipeline, VK_PIPELINE_BIND_POINT_GRAPHICS);
		commandBuffer.SetViewport(0.0f, 0.0f, (float)extend.width, (float)extend.height, 0.0f, 1.0f);
		commandBuffer.SetScissor({ 0, 0 }, extend);
		auto& meshContainer = engineContext->GetMeshContainer();
		
		VkBuffer vertexBuffers[] = { meshContainer.GetVertexBuffer() };
		VkDeviceSize offsets[] = { 0 };
		commandBuffer.BindVertexBuffers(0, 1, vertexBuffers, offsets);
		commandBuffer.BindIndexBuffer(meshContainer.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
		commandBuffer.BindDescriptorSers(VK_PIPELINE_BIND_POINT_GRAPHICS, *mPipeline, 0, 1, mDescriptorSets[frameIndex].GetNativeSet(), 0, nullptr);

		for (auto& object : engineContext->GetScene().GetRenderableObjects())
		{
			commandBuffer.PushConstants(mPipeline->GetNativePipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ForwardPassPushConstants), &object.objMatrix);
			auto& mesh = meshContainer.GetMesh(object.meshIndex);
			commandBuffer.DrawIndexed(mesh.GetIndexCount(), 1, mesh.GetFirstIndex(), mesh.GetVertexOffset(), 0);
		}
		commandBuffer.EndRenderPass();
		
	}

	ForwardPass::ForwardPass(const std::unique_ptr<RenderContext>& context, const std::unique_ptr<ResourceManager>& resourceManager)
	{
		auto& vulkanContext = context->GetVulkanContext();
		auto& device = vulkanContext->GetDevice();
		ForwardPassInfo info{ resourceManager->GetColorBuffer()->GetFormat(), resourceManager->GetDepthBuffer()->GetFormat()};
		mRenderPass = std::make_unique<RenderPass>(device, info.GetInfo());

		DescriptorInfo uboInfo;
		uboInfo.bindingNumber = 0;
		uboInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboInfo.shaderStage = static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		std::vector<DescriptorInfo> bindingUBOs = { uboInfo };
		DescriptorInfo samplerInfo;
		samplerInfo.bindingNumber = 1;
		samplerInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerInfo.shaderStage = VK_SHADER_STAGE_FRAGMENT_BIT;
		std::vector<DescriptorInfo> bindingSamplers = { samplerInfo };
		mDescriptorSetLayout = std::make_unique<DescriptorSetLayout>(device, bindingUBOs, bindingSamplers);

		std::unique_ptr<PipelineInfo> pipelineInfo = std::make_unique<ForwardPipelineInfo>(device);

		std::vector<VkDescriptorSetLayout> descriptorLayouts = { mDescriptorSetLayout->GetNativeLayout() };
		VkPushConstantRange pushConstant;
		pushConstant.offset = 0;
		pushConstant.size = sizeof(ForwardPassPushConstants);
		pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		std::vector<VkPushConstantRange> pushConstants = { pushConstant };
		mPipeline = std::make_unique<Pipeline>(device, mRenderPass, descriptorLayouts, pushConstants, pipelineInfo);

		mUniformBuffers.clear();
		mDescriptorSets.clear();

		std::vector<VkImageView> attacments = { resourceManager->GetColorBuffer()->GetNativeView(), resourceManager->GetDepthBuffer()->GetNativeView()};
		 mFrameBuffer = std::make_unique<FrameBuffer>(
			device,
			attacments,
			vulkanContext->GetSwapChain().GetSwapChainExtent(),
			mRenderPass);

		auto& engineContext = context->GetEngineContext();
		auto& materialImage = engineContext->GetTextureContainer().GetImage(0);
		auto& materalSampler = engineContext->GetTextureContainer().GetSampler(SamplerType::Linear);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			UBO uniformBuffer(device);
			mUniformBuffers.push_back(std::move(uniformBuffer));

			DescriptorSet descriptorSet(
				device,
				vulkanContext->GetDescriptorPool(),
				*mDescriptorSetLayout, 
				mUniformBuffers[i].GetBuffer(),
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

		mFrameBuffer->Cleanup(vulkanContext->GetDevice());

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			mDescriptorSets[i].Cleanup(vulkanContext->GetDevice(), vulkanContext->GetDescriptorPool());
			mUniformBuffers[i].Cleanup(vulkanContext->GetDevice());
		}

		mPipeline->Cleanup(vulkanContext->GetDevice());
		mDescriptorSetLayout->Cleanup(vulkanContext->GetDevice());
		mRenderPass->Cleanup(vulkanContext->GetDevice());

	}

	void ForwardPass::ResizeResources(const std::unique_ptr<RenderContext>& context, const std::unique_ptr<ResourceManager>& resourceManager)
	{
		auto& vulkanContext = context->GetVulkanContext();
		mFrameBuffer->Cleanup(vulkanContext->GetDevice());

		std::vector<VkImageView> attacments = { resourceManager->GetColorBuffer()->GetNativeView(), resourceManager->GetDepthBuffer()->GetNativeView() };
		mFrameBuffer = std::make_unique<FrameBuffer>(
			vulkanContext->GetDevice(),
			attacments,
			vulkanContext->GetSwapChain().GetSwapChainExtent(),
			mRenderPass);
	}


}


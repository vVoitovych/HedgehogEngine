#include "ForwardPass.hpp"
#include "ForwardPassInfo.hpp"
#include "ForwardPipelineInfo.hpp"
#include "ForwardPassPushConstants.hpp"

#include "Context/RenderContext.hpp"
#include "Context/EngineContext.hpp"
#include "Context/ThreadContext.hpp"
#include "Context/VulkanContext.hpp"
#include "Context/FrameContext.hpp"

#include "ResourceManager/ResourceManager.hpp"
#include "Wrappeers/Device/Device.hpp"
#include "Wrappeers/SwapChain/SwapChain.hpp"
#include "Wrappeers/RenderPass/RenderPass.hpp"
#include "Wrappeers/Commands/CommandBuffer.hpp"
#include "Wrappeers/FrameBuffer/FrameBuffer.hpp"
#include "Wrappeers/RenderPass/RenderPass.hpp"
#include "Wrappeers/Descriptors/DescriptorLayoutBuilder.hpp"
#include "Wrappeers/Descriptors/DescriptorSetLayout.hpp"
#include "Wrappeers/Descriptors/DescriptorAllocator.hpp"
#include "Wrappeers/Pipeline/Pipeline.hpp"
#include "Wrappeers/Descriptors/DescriptorSet.hpp"
#include "Wrappeers/Descriptors/UBO.hpp"
#include "Wrappeers/Resources/Image/Image.hpp"
#include "Wrappeers/Resources/Sampler/Sampler.hpp"

#include "Containers/MeshContainer.hpp"
#include "Containers/MaterialContainer.hpp"
#include "Containers/DrawListContainer.hpp"
#include "Common/RendererSettings.hpp"

#include "Scene/Scene.hpp"

#include "Logger/Logger.hpp"

namespace Renderer
{
	void ForwardPass::Render(RenderContext& context, const ResourceManager& resourceManager)
	{
		auto& frameContext = context.GetFrameContext();
		auto& threadContext = context.GetThreadContext();
		auto& vulkanContext = context.GetVulkanContext();
		auto& engineContext = context.GetEngineContext();

		auto& materialContainer = engineContext.GetMaterialContainer();
		auto& drawListContainer = engineContext.GetDrawListContainer();

		auto& commandBuffer = threadContext.GetCommandBuffer();
		auto extend = vulkanContext.GetSwapChain().GetSwapChainExtent();
		auto backBufferIndex = frameContext.GetBackBufferIndex();
				
		commandBuffer.BeginRenderPass(extend, *mRenderPass, mFrameBuffer->GetNativeFrameBuffer());
		commandBuffer.BindPipeline(*mPipeline, VK_PIPELINE_BIND_POINT_GRAPHICS);
		commandBuffer.SetViewport(0.0f, 0.0f, (float)extend.width, (float)extend.height, 0.0f, 1.0f);
		commandBuffer.SetScissor({ 0, 0 }, extend);

		auto& meshContainer = engineContext.GetMeshContainer();		
		VkBuffer vertexBuffers[] = { meshContainer.GetVertexBuffer() };
		VkDeviceSize offsets[] = { 0 };
		commandBuffer.BindVertexBuffers(0, 1, vertexBuffers, offsets);
		commandBuffer.BindIndexBuffer(meshContainer.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

		commandBuffer.BindDescriptorSers(VK_PIPELINE_BIND_POINT_GRAPHICS, *mPipeline, 0, 1, threadContext.GetDescriptorSet().GetNativeSet(), 0, nullptr);
		auto& opaqueDrawList = drawListContainer.GetOpaqueList();
		for(auto& drawNode : opaqueDrawList)
		{ 
			auto& descriptorSet = materialContainer.GetDescriptorSet(drawNode.materialIndex);
			commandBuffer.BindDescriptorSers(VK_PIPELINE_BIND_POINT_GRAPHICS, *mPipeline, 1, 1, descriptorSet.GetNativeSet(), 0, nullptr);
			for (auto& object : drawNode.objects)
			{
				commandBuffer.PushConstants(mPipeline->GetNativePipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ForwardPassPushConstants), &object.objMatrix);
				auto& mesh = meshContainer.GetMesh(object.meshIndex);
				commandBuffer.DrawIndexed(mesh.GetIndexCount(), 1, mesh.GetFirstIndex(), mesh.GetVertexOffset(), 0);
			}
		}
		commandBuffer.EndRenderPass();
		
	}

	ForwardPass::ForwardPass(const RenderContext& context, const ResourceManager& resourceManager)
	{
		auto& vulkanContext = context.GetVulkanContext();
		auto& threadContext = context.GetThreadContext();
		auto& engineContext = context.GetEngineContext();

		auto& materialContainer = engineContext.GetMaterialContainer();

		auto& device = vulkanContext.GetDevice();
		ForwardPassInfo info{ resourceManager.GetColorBuffer().GetFormat(), resourceManager.GetDepthBuffer().GetFormat()};
		mRenderPass = std::make_unique<RenderPass>(device, info.GetInfo());

		std::unique_ptr<PipelineInfo> pipelineInfo = std::make_unique<ForwardPipelineInfo>(device);
		std::vector<VkDescriptorSetLayout> descriptorLayouts = { threadContext.GetLayout().GetNativeLayout(), materialContainer.GetDescriptorSetLayout().GetNativeLayout()};

		VkPushConstantRange pushConstant;
		pushConstant.offset = 0;
		pushConstant.size = sizeof(ForwardPassPushConstants);
		pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		std::vector<VkPushConstantRange> pushConstants = { pushConstant };
		mPipeline = std::make_unique<Pipeline>(device, *mRenderPass, descriptorLayouts, pushConstants, *pipelineInfo);

		std::vector<VkImageView> attacments = { resourceManager.GetColorBuffer().GetNativeView(), resourceManager.GetDepthBuffer().GetNativeView()};
		 mFrameBuffer = std::make_unique<FrameBuffer>(
			device,
			attacments,
			vulkanContext.GetSwapChain().GetSwapChainExtent(),
			*mRenderPass);

		pipelineInfo->Cleanup(vulkanContext.GetDevice());
	}

	ForwardPass::~ForwardPass()
	{
	}

	void ForwardPass::Cleanup(const RenderContext& context)
	{
		auto& vulkanContext = context.GetVulkanContext();

		mPipeline->Cleanup(vulkanContext.GetDevice());
		mFrameBuffer->Cleanup(vulkanContext.GetDevice());
		mRenderPass->Cleanup(vulkanContext.GetDevice());

	}

	void ForwardPass::ResizeResources(const RenderContext& context, const ResourceManager& resourceManager)
	{
		auto& vulkanContext = context.GetVulkanContext();
		mFrameBuffer->Cleanup(vulkanContext.GetDevice());

		std::vector<VkImageView> attacments = { resourceManager.GetColorBuffer().GetNativeView(), resourceManager.GetDepthBuffer().GetNativeView() };
		mFrameBuffer = std::make_unique<FrameBuffer>(
			vulkanContext.GetDevice(),
			attacments,
			vulkanContext.GetSwapChain().GetSwapChainExtent(),
			*mRenderPass);
	}


}


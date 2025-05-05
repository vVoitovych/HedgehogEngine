#include "ForwardPass.hpp"
#include "ForwardPassInfo.hpp"
#include "ForwardPipelineInfo.hpp"
#include "ForwardPassPushConstants.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"
#include "HedgehogContext/Context/ThreadContext.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogContext/Context/FrameContext.hpp"

#include "HedgehogRenderer/ResourceManager/ResourceManager.hpp"
#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogWrappers/Wrappeers/SwapChain/SwapChain.hpp"
#include "HedgehogWrappers/Wrappeers/RenderPass/RenderPass.hpp"
#include "HedgehogWrappers/Wrappeers/Commands/CommandBuffer.hpp"
#include "HedgehogWrappers/Wrappeers/FrameBuffer/FrameBuffer.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorLayoutBuilder.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorSetLayout.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorAllocator.hpp"
#include "HedgehogWrappers/Wrappeers/Pipeline/Pipeline.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorSet.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/UBO.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Image/Image.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Sampler/Sampler.hpp"

#include "HedgehogContext/Containers/MeshContainer/MeshContainer.hpp"
#include "HedgehogContext/Containers/MeshContainer/Mesh.hpp"
#include "HedgehogContext/Containers/MaterialContainer/MaterialContainer.hpp"
#include "HedgehogContext/Containers/DrawListContrainer/DrawListContainer.hpp"
#include "HedgehogCommon/Common/RendererSettings.hpp"

#include "Scene/Scene.hpp"

#include "Logger/Logger.hpp"

namespace Renderer
{
	void ForwardPass::Render(Context::Context& context, const ResourceManager& resourceManager)
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
		VkDeviceSize offsets[] = { 0 };
		VkBuffer positionBuffers[] = { meshContainer.GetPositionsBuffer() };
		VkBuffer texCoordsBuffers[] = { meshContainer.GetTexCoordsBuffer() };
		VkBuffer normalsBuffers[] = { meshContainer.GetNormalsBuffer() };

		commandBuffer.BindVertexBuffers(0, 1, positionBuffers, offsets);
		commandBuffer.BindVertexBuffers(1, 1, texCoordsBuffers, offsets);
		commandBuffer.BindVertexBuffers(2, 1, normalsBuffers, offsets);
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

	ForwardPass::ForwardPass(const Context::Context& context, const ResourceManager& resourceManager)
	{
		auto& vulkanContext = context.GetVulkanContext();
		auto& threadContext = context.GetThreadContext();
		auto& engineContext = context.GetEngineContext();

		auto& materialContainer = engineContext.GetMaterialContainer();

		auto& device = vulkanContext.GetDevice();
		ForwardPassInfo info{ resourceManager.GetColorBuffer().GetFormat(), resourceManager.GetDepthBuffer().GetFormat()};
		mRenderPass = std::make_unique<Wrappers::RenderPass>(device, info.GetInfo());

		std::unique_ptr<Wrappers::PipelineInfo> pipelineInfo = std::make_unique<ForwardPipelineInfo>(device);
		std::vector<VkDescriptorSetLayout> descriptorLayouts = { threadContext.GetLayout().GetNativeLayout(), materialContainer.GetDescriptorSetLayout().GetNativeLayout()};

		VkPushConstantRange pushConstant;
		pushConstant.offset = 0;
		pushConstant.size = sizeof(ForwardPassPushConstants);
		pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		std::vector<VkPushConstantRange> pushConstants = { pushConstant };
		mPipeline = std::make_unique<Wrappers::Pipeline>(device, *mRenderPass, descriptorLayouts, pushConstants, *pipelineInfo);

		std::vector<VkImageView> attacments = { resourceManager.GetColorBuffer().GetNativeView(), resourceManager.GetDepthBuffer().GetNativeView()};
		 mFrameBuffer = std::make_unique<Wrappers::FrameBuffer>(
			device,
			attacments,
			vulkanContext.GetSwapChain().GetSwapChainExtent(),
			*mRenderPass);

		pipelineInfo->Cleanup(vulkanContext.GetDevice());
	}

	ForwardPass::~ForwardPass()
	{
	}

	void ForwardPass::Cleanup(const Context::Context& context)
	{
		auto& vulkanContext = context.GetVulkanContext();

		mPipeline->Cleanup(vulkanContext.GetDevice());
		mFrameBuffer->Cleanup(vulkanContext.GetDevice());
		mRenderPass->Cleanup(vulkanContext.GetDevice());

	}

	void ForwardPass::ResizeResources(const Context::Context& context, const ResourceManager& resourceManager)
	{
		auto& vulkanContext = context.GetVulkanContext();
		mFrameBuffer->Cleanup(vulkanContext.GetDevice());

		std::vector<VkImageView> attacments = { resourceManager.GetColorBuffer().GetNativeView(), resourceManager.GetDepthBuffer().GetNativeView() };
		mFrameBuffer = std::make_unique<Wrappers::FrameBuffer>(
			vulkanContext.GetDevice(),
			attacments,
			vulkanContext.GetSwapChain().GetSwapChainExtent(),
			*mRenderPass);
	}


}


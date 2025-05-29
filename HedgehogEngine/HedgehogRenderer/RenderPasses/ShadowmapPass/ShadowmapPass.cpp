#include "ShadowmapPass.hpp"
#include "ShadowmapPassInfo.hpp"
#include "ShadowmapPipelineInfo.hpp"
#include "ShadowmapPassPushConstants.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"
#include "HedgehogContext/Context/ThreadContext.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogContext/Context/FrameContext.hpp"

#include "HedgehogSettings/Settings/HedgehogSettings.hpp"
#include "HedgehogSettings/Settings/ShadowmapingSettings.hpp"

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
	void ShadowmapPass::Render(Context::Context& context, const ResourceManager& resourceManager)
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

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_RenderPass->GetNativeRenderPass();
		renderPassInfo.framebuffer = m_FrameBuffer->GetNativeFrameBuffer();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = extend;
		VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();
				
		commandBuffer.BeginRenderPass(renderPassInfo);
		commandBuffer.BindPipeline(*m_Pipeline, VK_PIPELINE_BIND_POINT_GRAPHICS);
		commandBuffer.SetViewport(0.0f, 0.0f, (float)extend.width, (float)extend.height, 0.0f, 1.0f);
		commandBuffer.SetScissor({ 0, 0 }, extend);

		auto& meshContainer = engineContext.GetMeshContainer();
		VkDeviceSize offsets[] = { 0, 0, 0 };
		VkBuffer buffers[] = { meshContainer.GetPositionsBuffer(), meshContainer.GetTexCoordsBuffer(), meshContainer.GetNormalsBuffer() };

		commandBuffer.BindVertexBuffers(0, 3, buffers, offsets);
		commandBuffer.BindIndexBuffer(meshContainer.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

		commandBuffer.BindDescriptorSers(VK_PIPELINE_BIND_POINT_GRAPHICS, *m_Pipeline, 0, 1, threadContext.GetDescriptorSet().GetNativeSet(), 0, nullptr);
		auto& opaqueDrawList = drawListContainer.GetOpaqueList();
		for(auto& drawNode : opaqueDrawList)
		{ 
			auto& descriptorSet = materialContainer.GetDescriptorSet(drawNode.materialIndex);
			commandBuffer.BindDescriptorSers(VK_PIPELINE_BIND_POINT_GRAPHICS, *m_Pipeline, 1, 1, descriptorSet.GetNativeSet(), 0, nullptr);
			for (auto& object : drawNode.objects)
			{
				commandBuffer.PushConstants(m_Pipeline->GetNativePipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowmapPassPushConstants), &object.objMatrix);
				auto& mesh = meshContainer.GetMesh(object.meshIndex);
				commandBuffer.DrawIndexed(mesh.GetIndexCount(), 1, mesh.GetFirstIndex(), mesh.GetVertexOffset(), 0);
			}
		}
		commandBuffer.EndRenderPass();
		
	}

	ShadowmapPass::ShadowmapPass(const Context::Context& context, const ResourceManager& resourceManager)
	{
		auto& vulkanContext = context.GetVulkanContext();
		auto& threadContext = context.GetThreadContext();
		auto& engineContext = context.GetEngineContext();

		auto& materialContainer = engineContext.GetMaterialContainer();

		auto& device = vulkanContext.GetDevice();
		ShadowmapPassInfo info{ resourceManager.GetDepthBuffer().GetFormat()};
		m_RenderPass = std::make_unique<Wrappers::RenderPass>(device, info.GetInfo());

		std::unique_ptr<Wrappers::PipelineInfo> pipelineInfo = std::make_unique<ShadowmapPipelineInfo>(device);
		std::vector<VkDescriptorSetLayout> descriptorLayouts = { threadContext.GetLayout().GetNativeLayout(), materialContainer.GetDescriptorSetLayout().GetNativeLayout()};

		VkPushConstantRange pushConstant;
		pushConstant.offset = 0;
		pushConstant.size = sizeof(ShadowmapPassPushConstants);
		pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		std::vector<VkPushConstantRange> pushConstants = { pushConstant };
		m_Pipeline = std::make_unique<Wrappers::Pipeline>(device, *m_RenderPass, descriptorLayouts, pushConstants, *pipelineInfo);

		std::vector<VkImageView> attacments = { resourceManager.GetColorBuffer().GetNativeView(), resourceManager.GetDepthBuffer().GetNativeView()};
		 m_FrameBuffer = std::make_unique<Wrappers::FrameBuffer>(
			device,
			attacments,
			vulkanContext.GetSwapChain().GetSwapChainExtent(),
			*m_RenderPass);

		pipelineInfo->Cleanup(vulkanContext.GetDevice());
	}

	ShadowmapPass::~ShadowmapPass()
	{
	}

	void ShadowmapPass::Cleanup(const Context::Context& context)
	{
		auto& vulkanContext = context.GetVulkanContext();

		m_Pipeline->Cleanup(vulkanContext.GetDevice());
		m_FrameBuffer->Cleanup(vulkanContext.GetDevice());
		m_RenderPass->Cleanup(vulkanContext.GetDevice());

	}

	void ShadowmapPass::UpdateResources(const Context::Context& context, const ResourceManager& resourceManager)
	{
		auto& vulkanContext = context.GetVulkanContext();
		if (m_FrameBuffer != nullptr)
			m_FrameBuffer->Cleanup(vulkanContext.GetDevice());

		std::vector<VkImageView> attacments = { resourceManager.GetShadowMap().GetNativeView() };
		auto& settings = context.GetEngineContext().GetSettings().GetShadowmapSettings();

		VkExtent2D extent = { settings->GetShadowmapSize(), settings->GetShadowmapSize() };
		m_FrameBuffer = std::make_unique<Wrappers::FrameBuffer>(
			vulkanContext.GetDevice(),
			attacments,
			extent,
			*m_RenderPass);
	}


}


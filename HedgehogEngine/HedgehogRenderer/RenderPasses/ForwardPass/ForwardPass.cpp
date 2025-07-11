#include "ForwardPass.hpp"
#include "ForwardPassInfo.hpp"
#include "ForwardPipelineInfo.hpp"
#include "ForwardPassPushConstants.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"
#include "HedgehogContext/Context/RendererContext.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogEngine/HedgehogCommon/Camera/Camera.hpp"

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
#include "HedgehogContext/Containers/LightContainer/LightContainer.hpp"

#include "HedgehogCommon/Common/RendererSettings.hpp"

#include "Scene/Scene.hpp"

#include "Logger/Logger.hpp"

namespace Renderer
{
	void ForwardPass::Render(Context::Context& context, const ResourceManager& resourceManager)
	{
		auto& rendererContext = context.GetRendererContext();
		auto& vulkanContext = context.GetVulkanContext();
		auto& engineContext = context.GetEngineContext();

		auto& materialContainer = engineContext.GetMaterialContainer();
		auto& drawListContainer = engineContext.GetDrawListContainer();

		auto& commandBuffer = rendererContext.GetCommandBuffer();
		auto extend = vulkanContext.GetSwapChain().GetSwapChainExtent();
		auto backBufferIndex = rendererContext.GetBackBufferIndex();
		auto frameIndex = rendererContext.GetFrameIndex();

		const auto& lightContainer = engineContext.GetLightContainer();

		const auto& camera = engineContext.GetCamera();
		ForwardPassFrameUniform ubo{};
		ubo.view = camera.GetViewMatrix();
		ubo.viewProj = camera.GetViewProjectionMatrix();
		ubo.eyePosition = HM::Vector4(camera.GetPosition(), 1.0f);
		ubo.lightCount = lightContainer.GetLightCount();
		const auto& lights = lightContainer.GetLights();
		for (size_t i = 0; i < ubo.lightCount; ++i)
		{
			ubo.lights[i] = lights[i];
		}
		m_FrameUniforms[frameIndex].UpdateUniformBuffer(ubo);

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

		commandBuffer.BindDescriptorSers(VK_PIPELINE_BIND_POINT_GRAPHICS, *m_Pipeline, 0, 1, m_FrameSets[frameIndex].GetNativeSet(), 0, nullptr);
		auto& opaqueDrawList = drawListContainer.GetOpaqueList();
		for(auto& drawNode : opaqueDrawList)
		{ 
			auto& descriptorSet = materialContainer.GetDescriptorSet(drawNode.materialIndex);
			commandBuffer.BindDescriptorSers(VK_PIPELINE_BIND_POINT_GRAPHICS, *m_Pipeline, 1, 1, descriptorSet.GetNativeSet(), 0, nullptr);
			for (auto& object : drawNode.objects)
			{
				commandBuffer.PushConstants(m_Pipeline->GetNativePipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ForwardPassPushConstants), &object.objMatrix);
				auto& mesh = meshContainer.GetMesh(object.meshIndex);
				commandBuffer.DrawIndexed(mesh.GetIndexCount(), 1, mesh.GetFirstIndex(), mesh.GetVertexOffset(), 0);
			}
		}
		commandBuffer.EndRenderPass();
		
	}

	ForwardPass::ForwardPass(const Context::Context& context, const ResourceManager& resourceManager)
	{
		auto& vulkanContext = context.GetVulkanContext();
		auto& engineContext = context.GetEngineContext();

		auto& materialContainer = engineContext.GetMaterialContainer();
		auto& device = vulkanContext.GetDevice();

		std::vector<Wrappers::PoolSizeRatio> sizes =
		{
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
		};

		m_FrameAllocator = std::make_unique<Wrappers::DescriptorAllocator>(device, MAX_FRAMES_IN_FLIGHT, sizes);

		Wrappers::DescriptorLayoutBuilder builder;
		builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		m_FrameLayout = std::make_unique<Wrappers::DescriptorSetLayout>(device, builder, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

		m_FrameUniforms.clear();
		m_FrameSets.clear();
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			Wrappers::UBO<ForwardPassFrameUniform> frameUniformBuffer(device);
			m_FrameUniforms.push_back(std::move(frameUniformBuffer));
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = m_FrameUniforms[i].GetNativeBuffer();
			bufferInfo.offset = 0;
			bufferInfo.range = m_FrameUniforms[i].GetBufferSize();

			Wrappers::DescriptorWrites write{};
			write.dstBinding = 0;
			write.dstArrayElement = 0;
			write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			write.descriptorCount = 1;
			write.pBufferInfo = &bufferInfo;
			write.pNext = nullptr;
			std::vector<Wrappers::DescriptorWrites> writes;
			writes.push_back(write);

			Wrappers::DescriptorSet descriptorSet(device, *m_FrameAllocator, *m_FrameLayout);
			descriptorSet.Update(device, writes);
			m_FrameSets.push_back(std::move(descriptorSet));
		}

		ForwardPassInfo info{ resourceManager.GetColorBuffer().GetFormat(), resourceManager.GetDepthBuffer().GetFormat()};
		m_RenderPass = std::make_unique<Wrappers::RenderPass>(device, info.GetInfo());

		std::unique_ptr<Wrappers::PipelineInfo> pipelineInfo = std::make_unique<ForwardPipelineInfo>(device);
		std::vector<VkDescriptorSetLayout> descriptorLayouts = { m_FrameLayout->GetNativeLayout(), materialContainer.GetDescriptorSetLayout().GetNativeLayout()};

		VkPushConstantRange pushConstant;
		pushConstant.offset = 0;
		pushConstant.size = sizeof(ForwardPassPushConstants);
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

	ForwardPass::~ForwardPass()
	{
	}

	void ForwardPass::Cleanup(const Context::Context& context)
	{
		auto& vulkanContext = context.GetVulkanContext();
		auto& device = vulkanContext.GetDevice();

		m_Pipeline->Cleanup(device);
		m_FrameBuffer->Cleanup(device);
		m_RenderPass->Cleanup(device);
		for (auto& frameUniform : m_FrameUniforms)
		{
			frameUniform.Cleanup(device);
		}
		m_FrameUniforms.clear();

		for (auto& frameSet : m_FrameSets)
		{
			frameSet.Cleanup(device, *m_FrameAllocator);
		}
		m_FrameSets.clear();

		m_FrameLayout->Cleanup(device);
		m_FrameAllocator->Cleanup(device);

	}

	void ForwardPass::ResizeResources(const Context::Context& context, const ResourceManager& resourceManager)
	{
		auto& vulkanContext = context.GetVulkanContext();
		m_FrameBuffer->Cleanup(vulkanContext.GetDevice());

		std::vector<VkImageView> attacments = { resourceManager.GetColorBuffer().GetNativeView(), resourceManager.GetDepthBuffer().GetNativeView() };
		m_FrameBuffer = std::make_unique<Wrappers::FrameBuffer>(
			vulkanContext.GetDevice(),
			attacments,
			vulkanContext.GetSwapChain().GetSwapChainExtent(),
			*m_RenderPass);
	}


}


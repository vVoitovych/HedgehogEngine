#include "ShadowmapPass.hpp"
#include "ShadowmapPassInfo.hpp"
#include "ShadowmapPipelineInfo.hpp"
#include "ShadowmapPassPushConstants.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"
#include "HedgehogContext/Context/ThreadContext.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"

#include "HedgehogCommon/Camera/Camera.hpp"

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

#include <algorithm>

namespace Renderer
{
	void ShadowmapPass::Render(Context::Context& context, const ResourceManager& resourceManager)
	{
		auto& threadContext = context.GetThreadContext();
		auto& engineContext = context.GetEngineContext();
		auto& settings = context.GetEngineContext().GetSettings().GetShadowmapSettings();
		uint32_t frameIndex = threadContext.GetFrameIndex();

		auto& drawListContainer = engineContext.GetDrawListContainer();
		auto& commandBuffer = threadContext.GetCommandBuffer();

		std::array<VkClearValue, 1> clearValues{};
		clearValues[0].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_RenderPass->GetNativeRenderPass();
		renderPassInfo.framebuffer = m_FrameBuffer->GetNativeFrameBuffer();
		renderPassInfo.renderArea.offset = { 0, 0 };
		VkExtent2D extend = {settings->GetShadowmapSize(), settings->GetShadowmapSize()};
		renderPassInfo.renderArea.extent = extend;
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();
				
		commandBuffer.BeginRenderPass(renderPassInfo);
		commandBuffer.BindPipeline(*m_Pipeline, VK_PIPELINE_BIND_POINT_GRAPHICS);
		auto cascadesCount = settings->GetCascadesCount();

		for (size_t i = 0; i < cascadesCount; ++i)
		{
			auto& view = m_ShadowViewports[cascadesCount - 1][i];
			commandBuffer.SetViewport(view.x, view.y, view.width, view.height, 0.0f, 1.0f);
			commandBuffer.SetScissor({ 0, 0 }, extend);

			auto& meshContainer = engineContext.GetMeshContainer();
			VkDeviceSize offsets[] = { 0, 0, 0 };
			VkBuffer buffers[] = { meshContainer.GetPositionsBuffer(), meshContainer.GetTexCoordsBuffer(), meshContainer.GetNormalsBuffer() };

			commandBuffer.BindVertexBuffers(0, 3, buffers, offsets);
			commandBuffer.BindIndexBuffer(meshContainer.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

			commandBuffer.BindDescriptorSers(VK_PIPELINE_BIND_POINT_GRAPHICS, *m_Pipeline, 0, 1, m_ShadowmapSets[frameIndex][i].GetNativeSet(), 0, nullptr);
			auto& opaqueDrawList = drawListContainer.GetOpaqueList();
			for (auto& drawNode : opaqueDrawList)
			{
				for (auto& object : drawNode.objects)
				{
					commandBuffer.PushConstants(m_Pipeline->GetNativePipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowmapPassPushConstants), &object.objMatrix);
					auto& mesh = meshContainer.GetMesh(object.meshIndex);
					commandBuffer.DrawIndexed(mesh.GetIndexCount(), 1, mesh.GetFirstIndex(), mesh.GetVertexOffset(), 0);
				}
			}
		}
		commandBuffer.EndRenderPass();
		
	}

	ShadowmapPass::ShadowmapPass(const Context::Context& context, const ResourceManager& resourceManager)
	{
		auto& vulkanContext = context.GetVulkanContext();
		auto& device = vulkanContext.GetDevice();

		CreateRenderPass(device, resourceManager);
		CreateAllocator(device);
		CreateLayout(device);
		CreateUniforms(device);
		CreateSets(device);
		CreatePipeline(device);
		UpdateFrameBuffer(context, resourceManager);
		UpdateViewports(context);

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

		for (auto& uniforms : m_ShadowmapUniforms)
		{
			for (auto& uniform : uniforms)
				uniform.Cleanup(vulkanContext.GetDevice());
		}
		m_ShadowmapUniforms.clear();

		for (auto& descSets : m_ShadowmapSets)
		{
			for (auto& descSet :descSets)
				descSet.Cleanup(vulkanContext.GetDevice(), *m_ShadowmapAllocator);
		}
		m_ShadowmapSets.clear();

		m_ShadowmapLayout->Cleanup(vulkanContext.GetDevice());
		m_ShadowmapAllocator->Cleanup(vulkanContext.GetDevice());

	}

	void ShadowmapPass::UpdateData(const Context::Context& context)
	{
		UpdateShadowmapMatricies(context);

		auto& settings = context.GetEngineContext().GetSettings().GetShadowmapSettings();
		auto index = context.GetThreadContext().GetFrameIndex();

		for (size_t i = 0; i < settings->GetCascadesCount(); ++i)
		{
			ShadowCascadeUniform ubo;
			ubo.shadowMatrix = m_ShadowmapMatricies[i];
			m_ShadowmapUniforms[index][i].UpdateUniformBuffer(ubo);
		}
	}

	void ShadowmapPass::UpdateResources(const Context::Context& context, const ResourceManager& resourceManager)
	{
		auto& settings = context.GetEngineContext().GetSettings().GetShadowmapSettings();
		if (!settings->IsDirty())
			return;

		UpdateFrameBuffer(context, resourceManager);
		UpdateViewports(context);
	}

	void ShadowmapPass::UpdateFrameBuffer(const Context::Context& context, const ResourceManager& resourceManager)
	{
		auto& settings = context.GetEngineContext().GetSettings().GetShadowmapSettings();

		auto& vulkanContext = context.GetVulkanContext();
		if (m_FrameBuffer != nullptr)
			m_FrameBuffer->Cleanup(vulkanContext.GetDevice());

		std::vector<VkImageView> attacments = { resourceManager.GetShadowMap().GetNativeView() };

		uint32_t shadowmapSize = (uint32_t)settings->GetShadowmapSize();
		VkExtent2D extent = { shadowmapSize, shadowmapSize };
		m_FrameBuffer = std::make_unique<Wrappers::FrameBuffer>(
			vulkanContext.GetDevice(),
			attacments,
			extent,
			*m_RenderPass);
	}

	void ShadowmapPass::CreateRenderPass(const Wrappers::Device& device, const ResourceManager& resourceManager)
	{
		ShadowmapPassInfo info{ resourceManager.GetDepthBuffer().GetFormat() };
		m_RenderPass = std::make_unique<Wrappers::RenderPass>(device, info.GetInfo());
	}

	void ShadowmapPass::CreateAllocator(const Wrappers::Device& device)
	{
		std::vector<Wrappers::PoolSizeRatio> sizes =
		{
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
		};

		m_ShadowmapAllocator = std::make_unique<Wrappers::DescriptorAllocator>(device, MAX_FRAMES_IN_FLIGHT * MaxShadowCascades, sizes);
	}

	void ShadowmapPass::CreateLayout(const Wrappers::Device& device)
	{
		Wrappers::DescriptorLayoutBuilder builder;
		builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		m_ShadowmapLayout = std::make_unique<Wrappers::DescriptorSetLayout>(device, builder, VK_SHADER_STAGE_VERTEX_BIT);
	}

	void ShadowmapPass::CreateUniforms(const Wrappers::Device& device)
	{
		m_ShadowmapUniforms.clear();
		m_ShadowmapUniforms.resize(MAX_FRAMES_IN_FLIGHT);
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			for (size_t j = 0; j < MaxShadowCascades; ++j)
			{
				Wrappers::UBO<ShadowCascadeUniform> frameUniformBuffer(device);
				m_ShadowmapUniforms[i].push_back(std::move(frameUniformBuffer));
			}
		}
	}

	void ShadowmapPass::CreateSets(const Wrappers::Device& device)
	{
		m_ShadowmapSets.clear();
		m_ShadowmapSets.resize(MAX_FRAMES_IN_FLIGHT);
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
			for (size_t j = 0; j < MaxShadowCascades; ++j)
			{
				VkDescriptorBufferInfo bufferInfo{};
				bufferInfo.buffer = m_ShadowmapUniforms[i][j].GetNativeBuffer();
				bufferInfo.offset = 0;
				bufferInfo.range = m_ShadowmapUniforms[i][j].GetBufferSize();

				Wrappers::DescriptorWrites write{};
				write.dstBinding = 0;
				write.dstArrayElement = 0;
				write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				write.descriptorCount = 1;
				write.pBufferInfo = &bufferInfo;
				write.pNext = nullptr;
				std::vector<Wrappers::DescriptorWrites> writes;
				writes.push_back(write);

				Wrappers::DescriptorSet descriptorSet(device, *m_ShadowmapAllocator, *m_ShadowmapLayout);
				descriptorSet.Update(device, writes);
				m_ShadowmapSets[i].push_back(std::move(descriptorSet));
			}
	}

	void ShadowmapPass::CreatePipeline(const Wrappers::Device& device)
	{
		std::unique_ptr<Wrappers::PipelineInfo> pipelineInfo = std::make_unique<ShadowmapPipelineInfo>(device);
		std::vector<VkDescriptorSetLayout> descriptorLayouts = { m_ShadowmapLayout->GetNativeLayout() };

		VkPushConstantRange pushConstant;
		pushConstant.offset = 0;
		pushConstant.size = sizeof(ShadowmapPassPushConstants);
		pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		std::vector<VkPushConstantRange> pushConstants = { pushConstant };
		m_Pipeline = std::make_unique<Wrappers::Pipeline>(device, *m_RenderPass, descriptorLayouts, pushConstants, *pipelineInfo);
		pipelineInfo->Cleanup(device);
	}

	void ShadowmapPass::UpdateShadowmapMatricies(const Context::Context& context)
	{
		auto& settings = context.GetEngineContext().GetSettings().GetShadowmapSettings();
		std::vector<float> cascadeSplits;

		auto& camera = context.GetEngineContext().GetCamera();
		float nearClip = camera.GetNearPlane();
		float farClip = camera.GetFarPlane();
		float clipRange = farClip - nearClip;

		float minZ = nearClip;
		float maxZ = nearClip + clipRange;

		float range = maxZ - minZ;
		float ratio = maxZ / minZ;

		// Calculate split depths based on view camera frustum
		// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
		uint32_t cascadesCount = settings->GetCascadesCount();
		float cascadeSplitLambda = settings->GetCascadeSplitLambda();

		for (uint32_t i = 0; i < cascadesCount; i++)
		{
			float p = (i + 1) / static_cast<float>(cascadesCount);
			float log = minZ * std::pow(ratio, p);
			float uniform = minZ + range * p;
			float d = cascadeSplitLambda * (log - uniform) + uniform;
			cascadeSplits.push_back((d - nearClip) / clipRange);
		}

		float lastSplitDist = 0.0;
		HM::Matrix4x4 camMatrix =  camera.GetViewMatrix() * camera.GetProjectionMatrix();
		bool succes = true;
		HM::Matrix4x4 invCam = camMatrix.Inverse(succes);

		for (uint32_t i = 0; i < cascadesCount; i++)
		{
			float splitDist = cascadeSplits[i];

			HM::Vector3 frustumCorners[8] = 
			{
				HM::Vector3(-1.0f,  1.0f, 0.0f),
				HM::Vector3(1.0f,  1.0f, 0.0f),
				HM::Vector3(1.0f, -1.0f, 0.0f),
				HM::Vector3(-1.0f, -1.0f, 0.0f),
				HM::Vector3(-1.0f,  1.0f,  1.0f),
				HM::Vector3(1.0f,  1.0f,  1.0f),
				HM::Vector3(1.0f, -1.0f,  1.0f),
				HM::Vector3(-1.0f, -1.0f,  1.0f),
			};


			for (uint32_t j = 0; j < 8; j++) 
			{
				HM::Vector4 invCorner = invCam * HM::Vector4(frustumCorners[j], 1.0f);
				frustumCorners[j] = invCorner / invCorner.w();
			}

			for (uint32_t j = 0; j < 4; j++) 
			{
				HM::Vector3 dist = frustumCorners[j + 4] - frustumCorners[j];
				frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
				frustumCorners[j] = frustumCorners[j] + (dist * lastSplitDist);
			}

			HM::Vector3 frustumCenter = HM::Vector3(0.0f, 0.0f, 0.0f);
			for (uint32_t j = 0; j < 8; j++) 
			{
				frustumCenter += frustumCorners[j];
			}
			frustumCenter /= 8.0f;

			float radius = 0.0f;
			for (uint32_t j = 0; j < 8; j++) 
			{
				HM::Vector3 diff = frustumCorners[j] - frustumCenter;
				float distance = diff.Length3Slow();
				radius = std::max(radius, distance);
			}
			radius = std::ceil(radius * 16.0f) / 16.0f;

			HM::Vector3 maxExtents = HM::Vector3({ radius, radius, radius });
			HM::Vector3 minExtents = -maxExtents;

			HM::Vector3 lightDir = HM::Vector3(1.0f, 0.0f, 0.0f); 
			const auto & shadowDir= context.GetEngineContext().GetScene().GetShadowLightDirection();
			if (shadowDir.has_value())
				lightDir = shadowDir.value();

			HM::Matrix4x4 lightViewMatrix = HM::Matrix4x4::LookAt(frustumCenter - lightDir * radius, frustumCenter, HM::Vector3(0.0f, 0.0f, 1.0f));
			HM::Matrix4x4 lightOrthoMatrix = HM::Matrix4x4::Ortho(minExtents.x(), maxExtents.x(), minExtents.y(), maxExtents.y(), 0.0f, maxExtents.z() - minExtents.z());

			m_ShadowmapMatricies[i] = lightOrthoMatrix * lightViewMatrix;

			lastSplitDist = cascadeSplits[i];
		}
	}

	void ShadowmapPass::UpdateViewports(const Context::Context& context)
	{
		auto& settings = context.GetEngineContext().GetSettings().GetShadowmapSettings();
		float shadowMapSize = static_cast<float>(settings->GetShadowmapSize());
		m_ShadowViewports.clear();
		m_ShadowViewports.resize(MaxShadowCascades);
		
		m_ShadowViewports[0].push_back({ 0.0f, 0.0f, shadowMapSize , shadowMapSize });

		m_ShadowViewports[1].push_back({ 0.0f, 0.0f, shadowMapSize / 2.0f, shadowMapSize });
		m_ShadowViewports[1].push_back({ shadowMapSize / 2.0f, 0.0f, shadowMapSize / 2.0f, shadowMapSize });


		m_ShadowViewports[2].push_back({ 0.0f, 0.0f, shadowMapSize / 2.0f, shadowMapSize });
		m_ShadowViewports[2].push_back({ shadowMapSize / 2.0f, 0.0f, shadowMapSize / 2.0f, shadowMapSize / 2.0f });
		m_ShadowViewports[2].push_back({ shadowMapSize / 2.0f, shadowMapSize / 2.0f, shadowMapSize / 2.0f, shadowMapSize / 2.0f });

		m_ShadowViewports[3].push_back({ 0.0f, 0.0f, shadowMapSize / 2.0f, shadowMapSize / 2.0f });
		m_ShadowViewports[3].push_back({ 0.0f, shadowMapSize / 2.0f, shadowMapSize / 2.0f, shadowMapSize / 2.0f });
		m_ShadowViewports[3].push_back({ shadowMapSize / 2.0f, 0.0f, shadowMapSize / 2.0f, shadowMapSize / 2.0f });
		m_ShadowViewports[3].push_back({ shadowMapSize / 2.0f, shadowMapSize / 2.0f, shadowMapSize / 2.0f, shadowMapSize / 2.0f });

	}


}


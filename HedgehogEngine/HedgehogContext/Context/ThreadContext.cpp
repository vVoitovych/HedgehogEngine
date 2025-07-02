#include "ThreadContext.hpp"
#include "VulkanContext.hpp"
#include "EngineContext.hpp"
#include "FrameContext.hpp"

#include "HedgehogWrappers/Wrappeers/Commands/CommandBuffer.hpp"
#include "HedgehogWrappers/Wrappeers/SyncObjects/SyncObject.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorAllocator.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorSetLayout.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/UBO.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Buffer/Buffer.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorSet.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorLayoutBuilder.hpp"

#include "HedgehogContext/Containers/LightContainer/LightContainer.hpp"
#include "HedgehogCommon/Common/EngineDebugBreak.hpp"
#include "HedgehogCommon/Common/RendererSettings.hpp"

#include "Logger/Logger.hpp"

#include <stdexcept>

namespace Context
{
	ThreadContext::ThreadContext(const VulkanContext& vulkanContext)
	{
		m_CommandBuffers.clear();
		m_SyncObjects.clear();
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			Wrappers::CommandBuffer commandBuffer(vulkanContext.GetDevice());
			m_CommandBuffers.push_back(std::move(commandBuffer));
			Wrappers::SyncObject syncObject(vulkanContext.GetDevice());
			m_SyncObjects.push_back(std::move(syncObject));
		}

		std::vector<Wrappers::PoolSizeRatio> sizes =
		{
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
		};

		m_FrameAllocator = std::make_unique<Wrappers::DescriptorAllocator>(vulkanContext.GetDevice(), MAX_FRAMES_IN_FLIGHT, sizes);

		Wrappers::DescriptorLayoutBuilder builder;
		builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		m_FrameLayout = std::make_unique<Wrappers::DescriptorSetLayout>(vulkanContext.GetDevice(), builder, VK_SHADER_STAGE_VERTEX_BIT  | VK_SHADER_STAGE_FRAGMENT_BIT);

		m_FrameUniforms.clear();
		m_FrameSets.clear();
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			Wrappers::UBO<FrameUniform> frameUniformBuffer(vulkanContext.GetDevice());
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

			Wrappers::DescriptorSet descriptorSet(vulkanContext.GetDevice(), *m_FrameAllocator, *m_FrameLayout);
			descriptorSet.Update(vulkanContext.GetDevice(), writes);
			m_FrameSets.push_back(std::move(descriptorSet));
		}
		LOGINFO("Thread context Initialized");
	}

	ThreadContext::~ThreadContext()
	{
	}

	void ThreadContext::Cleanup(const VulkanContext& vulkanContext)
	{
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			m_CommandBuffers[i].Cleanup(vulkanContext.GetDevice());
			m_SyncObjects[i].Cleanup(vulkanContext.GetDevice());
		}
		m_CommandBuffers.clear();
		m_SyncObjects.clear();

		for (auto& frameUniform : m_FrameUniforms)
		{
			frameUniform.Cleanup(vulkanContext.GetDevice());
		}
		m_FrameUniforms.clear();

		for (auto& frameSet : m_FrameSets)
		{
			frameSet.Cleanup(vulkanContext.GetDevice(), *m_FrameAllocator);
		}
		m_FrameSets.clear();

		m_FrameLayout->Cleanup(vulkanContext.GetDevice());
		m_FrameAllocator->Cleanup(vulkanContext.GetDevice());

		LOGINFO("Thread context cleaned");
	}

	void ThreadContext::Update(const EngineContext& engineContext, const FrameContext& frameContext)
	{
		const auto& lightContainer = engineContext.GetLightContainer();

		FrameUniform ubo{};
		ubo.view = frameContext.GetCameraViewMatrix();
		ubo.viewProj = frameContext.GetCameraProjMatrix() * frameContext.GetCameraViewMatrix();
		ubo.eyePosition = HM::Vector4(frameContext.GetCameraPosition(), 1.0f);
		ubo.lightCount = lightContainer.GetLightCount();
		const auto& lights = lightContainer.GetLights();
		for (size_t i = 0; i < ubo.lightCount; ++i)
		{
			ubo.lights[i] = lights[i];
		}
		m_FrameUniforms[m_FrameIndex].UpdateUniformBuffer(ubo);
	}

	void ThreadContext::NextFrame()
	{
		m_FrameIndex = (m_FrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	uint32_t ThreadContext::GetFrameIndex() const
	{
		return m_FrameIndex;
	}

	Wrappers::CommandBuffer& ThreadContext::GetCommandBuffer()
	{
		return m_CommandBuffers[m_FrameIndex];
	}

	Wrappers::SyncObject& ThreadContext::GetSyncObject()
	{
		return m_SyncObjects[m_FrameIndex];
	}

	const Wrappers::DescriptorSetLayout& ThreadContext::GetLayout() const
	{
		return *m_FrameLayout;
	}

	const Wrappers::DescriptorSet& ThreadContext::GetDescriptorSet() const
	{
		return m_FrameSets[m_FrameIndex];
	}

	Wrappers::DescriptorSet& ThreadContext::GetDescriptorSet()
	{
		return m_FrameSets[m_FrameIndex];
	}

}



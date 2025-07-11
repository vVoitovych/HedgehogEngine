#include "RendererContext.hpp"
#include "VulkanContext.hpp"
#include "EngineContext.hpp"

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
	RendererContext::RendererContext(const VulkanContext& vulkanContext)
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

		LOGINFO("Thread context Initialized");
	}

	RendererContext::~RendererContext()
	{
	}

	void RendererContext::Cleanup(const VulkanContext& vulkanContext)
	{
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			m_CommandBuffers[i].Cleanup(vulkanContext.GetDevice());
			m_SyncObjects[i].Cleanup(vulkanContext.GetDevice());
		}
		m_CommandBuffers.clear();
		m_SyncObjects.clear();

		LOGINFO("Thread context cleaned");
	}

	void RendererContext::NextFrame()
	{
		m_FrameIndex = (m_FrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	uint32_t RendererContext::GetFrameIndex() const
	{
		return m_FrameIndex;
	}

	void RendererContext::SetBackBufferIndex(uint32_t index)
	{
		m_BackBufferIndex = index;
	}

	uint32_t RendererContext::GetBackBufferIndex() const
	{
		return m_BackBufferIndex;
	}

	Wrappers::CommandBuffer& RendererContext::GetCommandBuffer()
	{
		return m_CommandBuffers[m_FrameIndex];
	}

	Wrappers::SyncObject& RendererContext::GetSyncObject()
	{
		return m_SyncObjects[m_FrameIndex];
	}

}



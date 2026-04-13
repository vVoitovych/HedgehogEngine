#include "SyncObject.hpp"
#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogCommon/Common/EngineDebugBreak.hpp"
#include "HedgehogCommon/Common/RendererSettings.hpp"

#include "Logger/Logger.hpp"

namespace Wrappers
{
	SyncObject::SyncObject(const Device& device)
		: m_ImageAvailableSemaphore(nullptr)
		, m_RendeerFinishedSemaphore(nullptr)
		, m_InFlightFence(nullptr)
	{
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		if (vkCreateSemaphore(device.GetNativeDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphore) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create image available semaphore!");
		}

		if (vkCreateSemaphore(device.GetNativeDevice(), &semaphoreInfo, nullptr, &m_RendeerFinishedSemaphore) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create render finished semaphore!");
		}

		if (vkCreateFence(device.GetNativeDevice(), &fenceInfo, nullptr, &m_InFlightFence) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create in flight fence!");
		}

		LOGINFO("Sync objects created");
	}

	SyncObject::~SyncObject()
	{
		if (m_ImageAvailableSemaphore != nullptr)
		{
			LOGERROR("Vulkan image available semaphore should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (m_RendeerFinishedSemaphore != nullptr)
		{
			LOGERROR("Vulkan render finished semaphore should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (m_InFlightFence != nullptr)
		{
			LOGERROR("Vulkan in flight fence should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	SyncObject::SyncObject(SyncObject&& other) noexcept
		: m_ImageAvailableSemaphore(other.m_ImageAvailableSemaphore)
		, m_RendeerFinishedSemaphore(other.m_RendeerFinishedSemaphore)
		, m_InFlightFence(other.m_InFlightFence)
	{
		other.m_ImageAvailableSemaphore = nullptr;
		other.m_RendeerFinishedSemaphore = nullptr;
		other.m_InFlightFence = nullptr;
	}

	SyncObject& SyncObject::operator=(SyncObject&& other) noexcept
	{
		if (this != &other)
		{
			m_ImageAvailableSemaphore = other.m_ImageAvailableSemaphore;
			m_RendeerFinishedSemaphore = other.m_RendeerFinishedSemaphore;
			m_InFlightFence = other.m_InFlightFence;

			other.m_ImageAvailableSemaphore = nullptr;
			other.m_RendeerFinishedSemaphore = nullptr;
			other.m_InFlightFence = nullptr;
		}
		return *this;
	}

	void SyncObject::Cleanup(const Device& device)
	{
		vkDestroySemaphore(device.GetNativeDevice(), m_ImageAvailableSemaphore, nullptr);
		vkDestroySemaphore(device.GetNativeDevice(), m_RendeerFinishedSemaphore, nullptr);
		vkDestroyFence(device.GetNativeDevice(), m_InFlightFence, nullptr);

		m_ImageAvailableSemaphore = nullptr;
		m_RendeerFinishedSemaphore = nullptr;
		m_InFlightFence = nullptr;
		LOGINFO("Sync objects cleaned");
	}

	VkSemaphore SyncObject::GetImageAvailableSemaphore()
	{
		return m_ImageAvailableSemaphore;
	}

	VkSemaphore SyncObject::GetRenderFinishedSemaphore()
	{
		return m_RendeerFinishedSemaphore;
	}

	VkFence SyncObject::GetInFlightFence()
	{
		return m_InFlightFence;
	}

	void SyncObject::WaitforInFlightFence(const Device& device)
	{
		vkWaitForFences(device.GetNativeDevice(), 1, &m_InFlightFence, VK_TRUE, UINT64_MAX);
	}

	void SyncObject::ResetInFlightFence(const Device& device)
	{
		vkResetFences(device.GetNativeDevice(), 1, &m_InFlightFence);
	}

}





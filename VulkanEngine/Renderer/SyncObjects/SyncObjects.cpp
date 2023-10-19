#include "SyncObjects.h"
#include "VulkanEngine/Renderer/Device/Device.h"
#include "VulkanEngine/Logger/Logger.h"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.h"
#include "VulkanEngine/Renderer/Common/RendererSettings.h"

namespace Renderer
{
	SyncObjects::SyncObjects()
	{
	}

	SyncObjects::~SyncObjects()
	{
		if (!mImageAvailableSemaphores.empty())
		{
			LOGERROR("Vulkan image available semaphores should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (!mRendeerFinishedSemaphores.empty())
		{
			LOGERROR("Vulkan render finished semaphores should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (!mInFlightFences.empty())
		{
			LOGERROR("Vulkan in flight fences should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void SyncObjects::Initialize(const Device& device)
	{
		mImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		mRendeerFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		mInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			if (vkCreateSemaphore(device.GetNativeDevice(), &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(device.GetNativeDevice(), &semaphoreInfo, nullptr, &mRendeerFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(device.GetNativeDevice(), &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create sync objects!");
			}
		}
		LOGINFO("Sync objects created");
	}

	void SyncObjects::Cleanup(const Device& device)
	{
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			vkDestroySemaphore(device.GetNativeDevice(), mImageAvailableSemaphores[i], nullptr);
			mImageAvailableSemaphores[i] = nullptr;
			vkDestroySemaphore(device.GetNativeDevice(), mRendeerFinishedSemaphores[i], nullptr);
			mRendeerFinishedSemaphores[i] = nullptr;
			vkDestroyFence(device.GetNativeDevice(), mInFlightFences[i], nullptr);
			mInFlightFences[i] = nullptr;
		}
		mImageAvailableSemaphores.clear();
		mRendeerFinishedSemaphores.clear();
		mInFlightFences.clear();
		LOGINFO("Sync objects cleaned");
	}

	VkSemaphore SyncObjects::GetImageAvailableSemaphore(size_t index) 
	{
		return mImageAvailableSemaphores[index];
	}

	VkSemaphore SyncObjects::GetRenderFinishedSemaphore(size_t index) 
	{
		return mRendeerFinishedSemaphores[index];
	}

	VkFence SyncObjects::GetInFlightFence(size_t index) 
	{
		return mInFlightFences[index];
	}

	void SyncObjects::WaitforInFlightFence(const Device& device, size_t index)
	{
		vkWaitForFences(device.GetNativeDevice(), 1, &mInFlightFences[index], VK_TRUE, UINT64_MAX);
	}

	void SyncObjects::ResetInFlightFence(const Device& device, size_t index)
	{
		vkResetFences(device.GetNativeDevice(), 1, &mInFlightFences[index]);
	}

}





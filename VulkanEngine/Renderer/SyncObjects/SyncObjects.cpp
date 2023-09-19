#include "SyncObjects.h"
#include "VulkanEngine/Renderer/Device/Device.h"
#include "VulkanEngine/Logger/Logger.h"

namespace Renderer
{
	SyncObjects::SyncObjects()
	{
	}

	SyncObjects::~SyncObjects()
	{
		if (!mImageAvailableSemaphores.empty())
		{
			throw std::runtime_error("Vulkan image available semaphores should be cleanedup before destruction!");
		}
		if (!mRendeerFinishedSemaphores.empty())
		{
			throw std::runtime_error("Vulkan render finished semaphores should be cleanedup before destruction!");
		}
		if (!mInFlightFences.empty())
		{
			throw std::runtime_error("Vulkan in flight fences should be cleanedup before destruction!");
		}
	}

	void SyncObjects::Initialize(Device& device)
	{
		mDevice = device.GetNativeDevice();

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
			if (vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mRendeerFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(mDevice, &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create sync objects!");
			}
		}
		LOGINFO("Sync objects created");
	}

	void SyncObjects::Cleanup()
	{
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			vkDestroySemaphore(mDevice, mImageAvailableSemaphores[i], nullptr);
			mImageAvailableSemaphores[i] = nullptr;
			vkDestroySemaphore(mDevice, mRendeerFinishedSemaphores[i], nullptr);
			mRendeerFinishedSemaphores[i] = nullptr;
			vkDestroyFence(mDevice, mInFlightFences[i], nullptr);
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

	void SyncObjects::WaitforInFlightFence(size_t index)
	{
		vkWaitForFences(mDevice, 1, &mInFlightFences[index], VK_TRUE, UINT64_MAX);
	}

	void SyncObjects::ResetInFlightFence(size_t index)
	{
		vkResetFences(mDevice, 1, &mInFlightFences[index]);
	}

}





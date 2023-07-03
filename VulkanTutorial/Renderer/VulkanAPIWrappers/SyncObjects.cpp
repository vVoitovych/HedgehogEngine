#include "SyncObjects.h"

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
			if (vkCreateSemaphore(device.GetDevice(), &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(device.GetDevice(), &semaphoreInfo, nullptr, &mRendeerFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(device.GetDevice(), &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create sync objects!");
			}
		}
	}

	void SyncObjects::Cleanup(Device& device)
	{
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			vkDestroySemaphore(device.GetDevice(), mImageAvailableSemaphores[i], nullptr);
			mImageAvailableSemaphores[i] = nullptr;
			vkDestroySemaphore(device.GetDevice(), mRendeerFinishedSemaphores[i], nullptr);
			mRendeerFinishedSemaphores[i] = nullptr;
			vkDestroyFence(device.GetDevice(), mInFlightFences[i], nullptr);
			mInFlightFences[i] = nullptr;
		}
		mImageAvailableSemaphores.clear();
		mRendeerFinishedSemaphores.clear();
		mInFlightFences.clear();
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

	void SyncObjects::WaitforInFlightFence(Device& device, size_t index)
	{
		vkWaitForFences(device.GetDevice(), 1, &mInFlightFences[index], VK_TRUE, UINT64_MAX);
		vkResetFences(device.GetDevice(), 1, &mInFlightFences[index]);
	}

}





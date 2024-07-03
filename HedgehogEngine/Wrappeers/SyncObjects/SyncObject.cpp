#include "SyncObject.hpp"
#include "Wrappeers/Device/Device.hpp"
#include "Common/EngineDebugBreak.hpp"
#include "Common/RendererSettings.hpp"

#include "Logger/Logger.hpp"

namespace Renderer
{
	SyncObject::SyncObject(const Device& device)
		: mImageAvailableSemaphore(nullptr)
		, mRendeerFinishedSemaphore(nullptr)
		, mInFlightFence(nullptr)
	{
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		if (vkCreateSemaphore(device.GetNativeDevice(), &semaphoreInfo, nullptr, &mImageAvailableSemaphore) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create image available semaphore!");
		}

		if (vkCreateSemaphore(device.GetNativeDevice(), &semaphoreInfo, nullptr, &mRendeerFinishedSemaphore) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create render finished semaphore!");
		}

		if (vkCreateFence(device.GetNativeDevice(), &fenceInfo, nullptr, &mInFlightFence) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create in flight fence!");
		}

		LOGINFO("Sync objects created");
	}

	SyncObject::~SyncObject()
	{
		if (mImageAvailableSemaphore != nullptr)
		{
			LOGERROR("Vulkan image available semaphore should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (mRendeerFinishedSemaphore != nullptr)
		{
			LOGERROR("Vulkan render finished semaphore should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (mInFlightFence != nullptr)
		{
			LOGERROR("Vulkan in flight fence should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	SyncObject::SyncObject(SyncObject&& other)
		: mImageAvailableSemaphore(other.mImageAvailableSemaphore)
		, mRendeerFinishedSemaphore(other.mRendeerFinishedSemaphore)
		, mInFlightFence(other.mInFlightFence)
	{
		other.mImageAvailableSemaphore = nullptr;
		other.mRendeerFinishedSemaphore = nullptr;
		other.mInFlightFence = nullptr;
	}

	SyncObject& SyncObject::operator=(SyncObject&& other)
	{
		if (this != &other)
		{
			mImageAvailableSemaphore = other.mImageAvailableSemaphore;
			mRendeerFinishedSemaphore = other.mRendeerFinishedSemaphore;
			mInFlightFence = other.mInFlightFence;

			other.mImageAvailableSemaphore = nullptr;
			other.mRendeerFinishedSemaphore = nullptr;
			other.mInFlightFence = nullptr;
		}
		return *this;
	}

	void SyncObject::Cleanup(const Device& device)
	{
		vkDestroySemaphore(device.GetNativeDevice(), mImageAvailableSemaphore, nullptr);
		vkDestroySemaphore(device.GetNativeDevice(), mRendeerFinishedSemaphore, nullptr);
		vkDestroyFence(device.GetNativeDevice(), mInFlightFence, nullptr);

		mImageAvailableSemaphore = nullptr;
		mRendeerFinishedSemaphore = nullptr;
		mInFlightFence = nullptr;
		LOGINFO("Sync objects cleaned");
	}

	VkSemaphore SyncObject::GetImageAvailableSemaphore()
	{
		return mImageAvailableSemaphore;
	}

	VkSemaphore SyncObject::GetRenderFinishedSemaphore()
	{
		return mRendeerFinishedSemaphore;
	}

	VkFence SyncObject::GetInFlightFence()
	{
		return mInFlightFence;
	}

	void SyncObject::WaitforInFlightFence(const Device& device)
	{
		vkWaitForFences(device.GetNativeDevice(), 1, &mInFlightFence, VK_TRUE, UINT64_MAX);
	}

	void SyncObject::ResetInFlightFence(const Device& device)
	{
		vkResetFences(device.GetNativeDevice(), 1, &mInFlightFence);
	}

}





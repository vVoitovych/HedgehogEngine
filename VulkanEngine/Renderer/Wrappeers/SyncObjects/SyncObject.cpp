#include "SyncObject.hpp"
#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Logger/Logger.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"
#include "Renderer/Common/RendererSettings.hpp"

namespace Renderer
{
	SyncObject::SyncObject(const std::unique_ptr<Device>& device)
		: mImageAvailableSemaphore(nullptr)
		, mRendeerFinishedSemaphore(nullptr)
		, mInFlightFence(nullptr)
		, mDevice(nullptr)
	{
		mDevice = device->GetNativeDevice();
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		if (vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphore) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create image available semaphore!");
		}

		if (vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mRendeerFinishedSemaphore) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create render finished semaphore!");
		}

		if (vkCreateFence(mDevice, &fenceInfo, nullptr, &mInFlightFence) != VK_SUCCESS)
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
		, mDevice(other.mDevice)
	{
		other.mImageAvailableSemaphore = nullptr;
		other.mRendeerFinishedSemaphore = nullptr;
		other.mInFlightFence = nullptr;
		other.mDevice = nullptr;
	}

	SyncObject& SyncObject::operator=(SyncObject&& other)
	{
		if (this != &other)
		{
			mImageAvailableSemaphore = other.mImageAvailableSemaphore;
			mRendeerFinishedSemaphore = other.mRendeerFinishedSemaphore;
			mInFlightFence = other.mInFlightFence;
			mDevice = other.mDevice;

			other.mImageAvailableSemaphore = nullptr;
			other.mRendeerFinishedSemaphore = nullptr;
			other.mInFlightFence = nullptr;
			other.mDevice = nullptr;
		}
		return *this;
	}

	void SyncObject::Cleanup()
	{
		vkDestroySemaphore(mDevice, mImageAvailableSemaphore, nullptr);
		vkDestroySemaphore(mDevice, mRendeerFinishedSemaphore, nullptr);
		vkDestroyFence(mDevice, mInFlightFence, nullptr);

		mImageAvailableSemaphore = nullptr;
		mRendeerFinishedSemaphore = nullptr;
		mInFlightFence = nullptr;
		mDevice = nullptr;
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

	void SyncObject::WaitforInFlightFence()
	{
		vkWaitForFences(mDevice, 1, &mInFlightFence, VK_TRUE, UINT64_MAX);
	}

	void SyncObject::ResetInFlightFence()
	{
		vkResetFences(mDevice, 1, &mInFlightFence);
	}

}





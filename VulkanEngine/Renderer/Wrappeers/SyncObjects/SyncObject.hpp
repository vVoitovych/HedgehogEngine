#pragma once

#include <vulkan/vulkan.h>
#include <memory>

namespace Renderer
{
	class Device;

	class SyncObject
	{
	public:
		SyncObject(const std::unique_ptr<Device>& device);
		~SyncObject();

		SyncObject(const SyncObject&) = delete;
		SyncObject& operator=(const SyncObject&) = delete;

		SyncObject(SyncObject&& other);
		SyncObject& operator=(SyncObject&& other);

		void Cleanup();

		VkSemaphore GetImageAvailableSemaphore();
		VkSemaphore GetRenderFinishedSemaphore();
		VkFence GetInFlightFence();

		void WaitforInFlightFence();
		void ResetInFlightFence();
	private:
		VkSemaphore mImageAvailableSemaphore;
		VkSemaphore mRendeerFinishedSemaphore;
		VkFence mInFlightFence;

		VkDevice mDevice;
	};

}




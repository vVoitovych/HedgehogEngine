#pragma once

#include <vulkan/vulkan.h>

namespace Hedgehog
{
	namespace Wrappers
	{
		class Device;

		class SyncObject
		{
		public:
			SyncObject(const Device& device);
			~SyncObject();

			SyncObject(const SyncObject&) = delete;
			SyncObject& operator=(const SyncObject&) = delete;

			SyncObject(SyncObject&& other);
			SyncObject& operator=(SyncObject&& other);

			void Cleanup(const Device& device);

			VkSemaphore GetImageAvailableSemaphore();
			VkSemaphore GetRenderFinishedSemaphore();
			VkFence GetInFlightFence();

			void WaitforInFlightFence(const Device& device);
			void ResetInFlightFence(const Device& device);
		private:
			VkSemaphore mImageAvailableSemaphore;
			VkSemaphore mRendeerFinishedSemaphore;
			VkFence mInFlightFence;

		};
	}
}




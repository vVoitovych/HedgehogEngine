#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace Renderer
{
	class Device;

	class SyncObjects
	{
	public:
		SyncObjects();
		~SyncObjects();

		SyncObjects(const SyncObjects&) = delete;
		SyncObjects& operator=(const SyncObjects&) = delete;

		void Initialize(Device& device);
		void Cleanup();

		VkSemaphore GetImageAvailableSemaphore(size_t index);
		VkSemaphore GetRenderFinishedSemaphore(size_t index);
		VkFence GetInFlightFence(size_t index);

		void WaitforInFlightFence(size_t index);
		void ResetInFlightFence(size_t index);
	private:
		VkDevice mDevice;

		std::vector<VkSemaphore> mImageAvailableSemaphores;
		std::vector<VkSemaphore> mRendeerFinishedSemaphores;
		std::vector<VkFence> mInFlightFences;
	};

}






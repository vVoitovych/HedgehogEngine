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

		void Initialize(const Device& device);
		void Cleanup(const Device& device);

		VkSemaphore GetImageAvailableSemaphore(size_t index);
		VkSemaphore GetRenderFinishedSemaphore(size_t index);
		VkFence GetInFlightFence(size_t index);

		void WaitforInFlightFence(const Device& device, size_t index);
		void ResetInFlightFence(const Device& device, size_t index);
	private:
		std::vector<VkSemaphore> mImageAvailableSemaphores;
		std::vector<VkSemaphore> mRendeerFinishedSemaphores;
		std::vector<VkFence> mInFlightFences;
	};

}






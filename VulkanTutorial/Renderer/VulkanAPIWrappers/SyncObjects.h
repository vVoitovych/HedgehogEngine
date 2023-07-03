#pragma once

#include "../Common/pch.h"
#include "Device.h"

namespace Renderer
{
	class SyncObjects
	{
	public:
		SyncObjects();
		~SyncObjects();

		SyncObjects(const SyncObjects&) = delete;
		SyncObjects& operator=(const SyncObjects&) = delete;

		void Initialize(Device& device);
		void Cleanup(Device& device);

		VkSemaphore GetImageAvailableSemaphore(size_t index);
		VkSemaphore GetRenderFinishedSemaphore(size_t index);
		VkFence GetInFlightFence(size_t index);

		void WaitforInFlightFence(Device& device, size_t index);
	private:
		std::vector<VkSemaphore> mImageAvailableSemaphores;
		std::vector<VkSemaphore> mRendeerFinishedSemaphores;
		std::vector<VkFence> mInFlightFences;
	};

}






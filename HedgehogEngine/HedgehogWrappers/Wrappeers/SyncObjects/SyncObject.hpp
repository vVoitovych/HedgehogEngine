#pragma once

#include <vulkan/vulkan.h>

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

		SyncObject(SyncObject&& other) noexcept;
		SyncObject& operator=(SyncObject&& other) noexcept;

		void Cleanup(const Device& device);

		VkSemaphore GetImageAvailableSemaphore();
		VkSemaphore GetRenderFinishedSemaphore();
		VkFence GetInFlightFence();

		void WaitforInFlightFence(const Device& device);
		void ResetInFlightFence(const Device& device);
	private:
		VkSemaphore m_ImageAvailableSemaphore;
		VkSemaphore m_RendeerFinishedSemaphore;
		VkFence m_InFlightFence;

	};

}




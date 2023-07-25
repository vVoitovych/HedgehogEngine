#pragma once

#include "../Common/pch.h"
#include "Instance.h"
#include "Surface.h"

namespace Renderer
{
	class CommandPool;

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> mGraphicsFamily;
		std::optional<uint32_t> mPresentFamily;

		bool IsComplete()
		{
			return mGraphicsFamily.has_value() && mPresentFamily.has_value();
		}
	};

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR mCapabilities;
		std::vector<VkSurfaceFormatKHR> mFormats;
		std::vector<VkPresentModeKHR> mPrecentModes;
	};

	class Device
	{
	public:
		Device();
		~Device();

		Device(const Device&) = delete;
		Device& operator=(const Device&) = delete;

		void Initialize(Instance& instance, Surface& surface);
		void Cleanup();

		VkQueue GetGraphicsQueue() const;
		VkQueue GetPresentQueue() const;
		VkDevice GetDevice() const;
		VkPhysicalDevice GetPhysicalDevice() const;
		QueueFamilyIndices GetIndicies() const;

		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

		void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
		void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, CommandPool& commandPool);

	private:
		void PickPhysicalDevice(Instance& instance, Surface& surface);
		void CreateLogicalDevice(Instance& instance, Surface& surface);

		bool CheckDeviceExtensionSupport(VkPhysicalDevice device, Instance& instance) const;
		bool IsDeviceSuitable(VkPhysicalDevice device, Instance& instance, Surface& surface) const;
		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, Instance& instance, Surface& surface) const;
		SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, Surface& surface) const;

	private:
		VkPhysicalDevice mPhysicalDevice;
		VkDevice mDevice;

		VkQueue mGraphicsQueue;
		VkQueue mPresentQueue;

		QueueFamilyIndices mIndices;
	};
}


#pragma once

#include "vma/vk_mem_alloc.h"

#include <vulkan/vulkan.h>
#include <optional>
#include <vector>
#include <memory>

namespace WinManager
{
	class WindowManager;
}

namespace Wrappers
{
	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		bool IsComplete()
		{
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> precentModes;
	};

	class Instance;
	class DebugMessager;

	class Device
	{
	public:
		Device(const WinManager::WindowManager& windowManager);
		~Device();

		Device(const Device&) = delete;
		Device& operator=(const Device&) = delete;
		Device(Device&&) = delete;
		Device& operator=(Device&&) = delete;

		void Cleanup();

		VkInstance GetNativeInstance() const;
		VkQueue GetNativeGraphicsQueue() const;
		VkQueue GetNativePresentQueue() const;
		VkSurfaceKHR GetNativeSurface() const ;
		VkDevice GetNativeDevice() const;
		VkPhysicalDevice GetNativePhysicalDevice() const;
		QueueFamilyIndices GetIndicies() const;
		const VmaAllocator& GetAllocator() const;

		void SetObjectName(uint64_t objectHandle, VkObjectType objectType, const char* name) const;

	public:
		SwapChainSupportDetails QuerySwapChainSupport() const;
		VkPhysicalDeviceProperties GetPhysicalDeviceProperties() const;
		VkFormat FindDepthFormat() const;

		void AllocateCommandBuffer(VkCommandBuffer* pCommandBuffer) const;
		void FreeCommandBuffer(VkCommandBuffer* pCommandBuffer) const;

		void CopyBufferToBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const;
		void CopyBufferToImage(VkBuffer srcBuffer, VkImage image, uint32_t width, uint32_t height) const;
		void TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) const;

		void UpdateDescriptorSets(std::vector<VkWriteDescriptorSet>& descriptorWrites) const;
	private:
		void InitLayersAndExtentions();
		void PickPhysicalDevice();
		void CreateLogicalDevice();
		void InitializeAllocator();
		void InitializeCommandPool();

		bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;
		int GetDeviceScore(VkPhysicalDevice device) const;

		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
		VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;

	private:
		std::unique_ptr<Instance> m_Instance;
		std::unique_ptr<DebugMessager> m_DebugMessenger;
		VkSurfaceKHR m_Surface;
		VkPhysicalDevice m_PhysicalDevice;
		VkDevice m_Device;

		VkQueue m_GraphicsQueue;
		VkQueue m_PresentQueue;

		QueueFamilyIndices m_Indices;

		VmaAllocator m_Allocator;
		VkCommandPool m_CommandPool;

		std::vector<const char*> m_DeviceExtensions;
	};
}


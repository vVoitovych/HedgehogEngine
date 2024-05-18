#pragma once

#include "ThirdParty/VulkanMemoryAllocator/vk_mem_alloc.h"

#include <vulkan/vulkan.h>
#include <optional>
#include <vector>

namespace Renderer
{
	class WindowManager;
	class DescriptorSetLayout;
	const class UBO;

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
		Device(const WindowManager& windowManager);
		~Device();

		Device(const Device&) = delete;
		Device& operator=(const Device&) = delete;

		void Cleanup();

		VkInstance GetNativeInstance() const;
		VkQueue GetNativeGraphicsQueue() const;
		VkQueue GetNativePresentQueue() const;
		VkSurfaceKHR GetNativeSurface() const ;
		VkDevice GetNativeDevice() const;
		VkPhysicalDevice GetNativePhysicalDevice() const;
		QueueFamilyIndices GetIndicies() const;
		const VmaAllocator& GetAllocator() const;
		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

		void SetObjectName(uint64_t objectHandle, VkObjectType objectType, const char* name) const;

	public:
		SwapChainSupportDetails QuerySwapChainSupport() const;
		VkPhysicalDeviceProperties GetPhysicalDeviceProperties() const;
		VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
		VkFormat FindDepthFormat() const;

		void AllocateCommandBuffer(VkCommandBuffer* pCommandBuffer) const;
		void FreeCommandBuffer(VkCommandBuffer* pCommandBuffer) const;

		void CopyBufferToBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const;
		void CopyBufferToImage(VkBuffer srcBuffer, VkImage image, uint32_t width, uint32_t height) const;
		void TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) const;

	private:
		void InitLayersAndExtentions();
		void InitializeInstance();
		void InitializeDebugMessanger();
		void CleanupDebugMessanger();
		void PickPhysicalDevice();
		void CreateLogicalDevice();
		void InitializeAllocator();
		void InitializeCommandPool();

		bool IsEnableValidationLayers() const;
		void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const;
		bool CheckValidationLayerSupport() const;
		std::vector<const char*> GetRequiredExtensions() const;
		void HasGflwRequiredInstanceExtensions() const;

		bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;
		bool IsDeviceSuitable(VkPhysicalDevice device) const;
		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const;
		SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device) const;

	private:
		VkInstance mInstance;
		VkDebugUtilsMessengerEXT mDebugMessenger;
		VkSurfaceKHR mSurface;
		VkPhysicalDevice mPhysicalDevice;
		VkDevice mDevice;

		VkQueue mGraphicsQueue;
		VkQueue mPresentQueue;

		QueueFamilyIndices mIndices;

		VmaAllocator mAllocator;
		VkCommandPool mCommandPool;

		std::vector<const char*> mValidationLayers;
		std::vector<const char*> mDeviceExtensions;
	};
}


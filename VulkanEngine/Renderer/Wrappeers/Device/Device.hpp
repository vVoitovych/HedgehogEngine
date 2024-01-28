#pragma once

#include <vulkan/vulkan.h>
#include <optional>
#include <vector>
#include <memory>

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
		Device(const std::unique_ptr<WindowManager>& windowManager);
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
		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

	public:
		SwapChainSupportDetails QuerySwapChainSupport() const;
		VkPhysicalDeviceProperties GetPhysicalDeviceProperties() const;
		VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
		VkFormat FindDepthFormat() const;

	private:
		void InitializeInstance();
		void InitializeDebugMessanger();
		void CleanupDebugMessanger();
		void PickPhysicalDevice();
		void CreateLogicalDevice();

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
#ifdef DEBUG
		const bool enableValidationLayers = true;
#else
		const bool enableValidationLayers = false;
#endif
		VkInstance mInstance;
		VkDebugUtilsMessengerEXT mDebugMessenger;
		VkSurfaceKHR mSurface;
		VkPhysicalDevice mPhysicalDevice;
		VkDevice mDevice;

		VkQueue mGraphicsQueue;
		VkQueue mPresentQueue;

		QueueFamilyIndices mIndices;
	};
}


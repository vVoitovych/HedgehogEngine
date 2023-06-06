#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "..\VkWindow.h"

// std lib
#include <vector>
#include <optional>
#include <memory>

namespace Renderer
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

	class VulkanWrapper
	{
	public:
		VulkanWrapper(std::shared_ptr<VkEngine::VkWindow> window);
		~VulkanWrapper();

		VulkanWrapper(const VulkanWrapper&) = delete;
		VulkanWrapper& operator=(const VulkanWrapper&) = delete;
	private:
		void InitVulkan();
		void Cleanup();

		void CreateInstance();
		void SetupDebugMessenger();
		void PickPhysicalDevice();
		void CreateLogicalDevice();
		void CreateSurface();
		void CreateSwapChain();
		void CreateImageViews();

	private:
		bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;
		bool CheckValidationLayerSupport() const;
		std::vector<const char*> GetRequiredExtensions() const;
		void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const;
		bool IsDeviceSuitable(VkPhysicalDevice device) const;
		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const;
		void HasGflwRequiredInstanceExtensions() const;
		SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device) const;
		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
		VkExtent2D ChooseSwapExtend(const VkSurfaceCapabilitiesKHR& capabilities) const;
	private:
#ifdef DEBUG
		const bool enableValidationLayers = true;
#else
		const bool enableValidationLayers = false;
#endif
		const std::vector<const char*> validationLayers = {	"VK_LAYER_KHRONOS_validation" };
		const std::vector<const char*> deviceExtensions = {	VK_KHR_SWAPCHAIN_EXTENSION_NAME	};

	private:
		std::weak_ptr<VkEngine::VkWindow> mWindow;

		VkInstance mInstance;
		VkDebugUtilsMessengerEXT mDebugMessenger;
		VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
		VkDevice mDevice;
		VkSurfaceKHR mSurface;
		VkSwapchainKHR mSwapChain;

		std::vector<VkImage> mSwapChainImages;
		VkFormat mSwapChainImageFormat;
		VkExtent2D mSwapChainExtent;

		std::vector<VkImageView> mSwapChainImageViews;

		VkQueue mGraphicsQueue;
		VkQueue mPresentQueue;
	};
}


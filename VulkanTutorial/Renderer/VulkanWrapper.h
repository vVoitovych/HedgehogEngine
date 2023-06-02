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
	private:
		bool CheckValidationLayerSupport() const;
		std::vector<const char*> GetRequiredExtensions() const;
		void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const;
		bool IsDeviceSuitable(VkPhysicalDevice device) const;
		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const;
		void HasGflwRequiredInstanceExtensions() const;

	private:
#ifdef DEBUG
		const bool enableValidationLayers = true;
#else
		const bool enableValidationLayers = false;
#endif
		const std::vector<const char*> validationLayers = {	"VK_LAYER_KHRONOS_validation" };

	private:
		std::weak_ptr<VkEngine::VkWindow> mWindow;

		VkInstance mInstance;
		VkDebugUtilsMessengerEXT mDebugMessenger;

		VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
		VkDevice mDevice;

		VkSurfaceKHR mSurface;

		VkQueue mGraphicsQueue;
		VkQueue mPresentQueue;
	};
}


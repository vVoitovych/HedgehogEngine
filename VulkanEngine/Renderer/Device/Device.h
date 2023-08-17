#pragma once

#include "../Common/pch.h"

namespace Renderer
{
	class WindowManager;

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

		void Initialize(WindowManager& windowManager);
		void Cleanup();

		VkQueue GetNativeGraphicsQueue() const;
		VkQueue GetNativePresentQueue() const;
		VkSurfaceKHR GetNativeSurface();
		VkDevice GetNativeDevice() const;
		VkPhysicalDevice GetNativePhysicalDevice() const;
		QueueFamilyIndices GetIndicies() const;

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


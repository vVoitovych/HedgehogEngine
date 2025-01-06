#pragma once

#include <vulkan/vulkan.h>
#include <vector>

struct GLFWwindow;

namespace WinManager
{
	class WindowManager;
}

namespace Wrappers
{
	class Device;

	class SwapChain
	{
	public:
		SwapChain(const Device& device, WinManager::WindowManager& windowManager);
		~SwapChain();

		SwapChain(const SwapChain&) = delete;
		SwapChain& operator=(const SwapChain&) = delete;

		void Cleanup(const Device& device);
		void Recreate(const Device& device);

		VkSwapchainKHR GetNativeSwapChain() const;

		VkFormat GetFormat() const;
		VkExtent2D GetSwapChainExtent() const;
		uint32_t GetMinImagesCount() const;
		size_t GetSwapChainImagesSize() const;

		VkImageView GetNativeSwapChainImageView(size_t index) const;

		VkImage GetSwapChainImage(size_t index) const;

	private:
		void CreateSwapChain(const Device& device);
		void CreateImageViews(const Device& device);

		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
		VkExtent2D ChooseSwapExtend(const VkSurfaceCapabilitiesKHR& capabilities) const;

	private:
		GLFWwindow* mWindow;

		VkSwapchainKHR mSwapChain;

		std::vector<VkImage> mSwapChainImages;
		VkFormat mSwapChainImageFormat;
		VkExtent2D mSwapChainExtent;
		uint32_t mMinImageCount;
		std::vector<VkImageView> mSwapChainImageViews;

	};
}

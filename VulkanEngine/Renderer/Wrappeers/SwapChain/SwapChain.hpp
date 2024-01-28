#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <memory>

namespace Renderer
{
	class Device;
	class WindowManager;

	class SwapChain
	{
	public:
		SwapChain(const std::unique_ptr<Device>& device, std::unique_ptr<WindowManager>& windowManager);
		~SwapChain();

		SwapChain(const SwapChain&) = delete;
		SwapChain& operator=(const SwapChain&) = delete;

		void Cleanup(const std::unique_ptr<Device>& device);
		void Recreate(const std::unique_ptr<Device>& device);

		VkSwapchainKHR GetNativeSwapChain() const;

		VkFormat GetFormat() const;
		VkExtent2D GetSwapChainExtent() const;
		uint32_t GetMinImagesCount() const;
		size_t GetSwapChainImagesSize() const;

		VkImageView GetNativeSwapChainImageView(size_t index) const;

	private:
		void CreateSwapChain(const std::unique_ptr<Device>& device);
		void CreateImageViews(const std::unique_ptr<Device>& device);

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

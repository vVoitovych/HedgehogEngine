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
		SwapChain(SwapChain&&) = delete;
		SwapChain& operator=(SwapChain&&) = delete;

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
		GLFWwindow* m_Window;

		VkSwapchainKHR m_SwapChain;

		std::vector<VkImage> m_SwapChainImages;
		VkFormat m_SwapChainImageFormat;
		VkExtent2D m_SwapChainExtent;
		uint32_t m_MinImageCount;
		std::vector<VkImageView> m_SwapChainImageViews;

	};
}

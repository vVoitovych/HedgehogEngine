#pragma once

#include <vulkan/vulkan.h>
#include <optional>
#include <vector>

namespace Renderer
{
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

		void Initialize(class WindowManager& windowManager);
		void Cleanup();

		VkQueue GetNativeGraphicsQueue() const;
		VkQueue GetNativePresentQueue() const;
		VkSurfaceKHR GetNativeSurface() const ;
		VkDevice GetNativeDevice() const;
		VkPhysicalDevice GetNativePhysicalDevice() const;
		QueueFamilyIndices GetIndicies() const;

	public:
		void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, 	VkDeviceMemory& bufferMemory) const;
		void CopyBuffer(VkBuffer srcBuffer,	VkBuffer dstBuffer,	VkDeviceSize size) const;

		void CopyDataToBufferMemory(VkDeviceMemory memory, const void* data, size_t size) const;
		void MapMemory(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) const;

		void DestroyBuffer(VkBuffer buffer, const VkAllocationCallbacks* callBacks) const;
		void FreeMemory(VkDeviceMemory memory, const VkAllocationCallbacks* callBacks) const;

		void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, 
			VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) const;
		void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const;
		void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) const;

		void DestroyImage(VkImage image, const VkAllocationCallbacks* callback) const;

		VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) const;

	public:
		void AllocateCommandBuffer(VkCommandBuffer* pCommandBuffer) const;
		void FreeCommandBuffer(VkCommandBuffer* pCommandBuffer) const;
	private:
		VkCommandBuffer BeginSingleTimeCommands() const;
		void EndSingleTimeCommands(VkCommandBuffer commandBuffer) const;
	public:
		void AllocateDescriptorSet(class DescriptorSetLayout& descriptorSetLayout, const class UBO& ubo, VkDescriptorSet* pDescriptorSet) const;
		void FreeDescriptorSet(VkDescriptorSet* pDescriptorSet) const;

	public:
		SwapChainSupportDetails QuerySwapChainSupport() const;
		VkPhysicalDeviceProperties GetPhysicalDeviceProperties() const;
		VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;

	private:
		void InitializeInstance();
		void InitializeDebugMessanger();
		void CleanupDebugMessanger();
		void PickPhysicalDevice();
		void CreateLogicalDevice();
		void CreateCommandPool();
		void CreateDescriptorPool();

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

		VkCommandPool mCommandPool;
		VkDescriptorPool mDescriptorPool;
	};
}


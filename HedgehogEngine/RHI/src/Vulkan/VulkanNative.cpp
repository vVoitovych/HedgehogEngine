#include "RHI/api/Vulkan/VulkanNative.hpp"

#include "VulkanCommandList.hpp"
#include "VulkanDevice.hpp"
#include "VulkanTexture.hpp"
#include "VulkanTypes.hpp"

namespace RHI::VulkanNative
{
    DeviceHandles GetDeviceHandles(const IRHIDevice& device)
    {
        const auto& vkDevice = static_cast<const VulkanDevice&>(device);
        DeviceHandles handles;
        handles.Instance            = vkDevice.GetInstance();
        handles.PhysicalDevice      = vkDevice.GetPhysicalDevice();
        handles.Device              = vkDevice.GetHandle();
        handles.GraphicsQueueFamily = vkDevice.GetQueueFamilyIndices().GraphicsFamily.value();
        handles.GraphicsQueue       = vkDevice.GetGraphicsQueue();
        return handles;
    }

    VkCommandBuffer GetCommandBuffer(const IRHICommandList& commandList)
    {
        return static_cast<const VulkanCommandList&>(commandList).GetHandle();
    }

    VkImageView GetImageView(const IRHITexture& texture)
    {
        return static_cast<const VulkanTexture&>(texture).GetViewHandle();
    }

    VkFormat ToVkFormat(Format format)
    {
        return VulkanTypes::ToVkFormat(format);
    }
}

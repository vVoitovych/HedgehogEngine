#pragma once

#include "RHI/api/RHITypes.hpp"

#include <Volk/volk.h>

#include <cstdint>

namespace RHI
{
    class IRHIDevice;
    class IRHICommandList;
    class IRHITexture;
}

// The Vulkan backend's native handles, for the few integrations that must call a third-party
// Vulkan library directly (the editor's ImGui renderer, RHIImGui). Everything else goes through the
// RHI interfaces; nothing here changes how the RHI objects behave, and the handles stay owned by
// them. Every function requires the object to come from the Vulkan backend.
namespace RHI::VulkanNative
{
    struct DeviceHandles
    {
        VkInstance       Instance            = VK_NULL_HANDLE;
        VkPhysicalDevice PhysicalDevice      = VK_NULL_HANDLE;
        VkDevice         Device              = VK_NULL_HANDLE;
        uint32_t         GraphicsQueueFamily = 0;
        VkQueue          GraphicsQueue       = VK_NULL_HANDLE;
    };

    [[nodiscard]] DeviceHandles   GetDeviceHandles(const IRHIDevice& device);
    [[nodiscard]] VkCommandBuffer GetCommandBuffer(const IRHICommandList& commandList);
    [[nodiscard]] VkImageView     GetImageView(const IRHITexture& texture);
    [[nodiscard]] VkFormat        ToVkFormat(Format format);
}

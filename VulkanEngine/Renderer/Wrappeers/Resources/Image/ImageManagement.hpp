#pragma once

#include <vulkan/vulkan.h>

namespace Renderer
{
	namespace ImageManagement
	{
        void RecordTransitionImageLayout(
            const uint32_t mipLevels,
            const VkImageLayout& oldLayout,
            const VkImageLayout& newLayout,
            const bool isCubemap,
            VkCommandBuffer commandBuffer,
            const VkImage& image
        );

        void CreateImageMemoryBarrier(
            const uint32_t mipLevels,
            const VkImageLayout& oldLayout,
            const VkImageLayout& newLayout,
            const bool isCubemap,
            const VkImage& image,
            VkImageMemoryBarrier& imgMemoryBarrier,
            VkPipelineStageFlags& sourceStage,
            VkPipelineStageFlags& destinationStage
        );
	}
}





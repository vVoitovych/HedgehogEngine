#include "ImageManagement.hpp"

#include <stdexcept>

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
			const VkImage& image)
		{
            VkImageMemoryBarrier imgMemoryBarrier{};
            VkPipelineStageFlags sourceStage, destinationStage;
            CreateImageMemoryBarrier(
                mipLevels,
                oldLayout,
                newLayout,
                isCubemap,
                image,
                imgMemoryBarrier,
                sourceStage,
                destinationStage
            );

            vkCmdPipelineBarrier(
                commandBuffer,
                sourceStage, destinationStage,
                0,
                0, nullptr,
                0, nullptr,
                1, &imgMemoryBarrier
            );

		}

		void CreateImageMemoryBarrier(
			const uint32_t mipLevels, 
			const VkImageLayout& oldLayout, 
			const VkImageLayout& newLayout, 
			const bool isCubemap, 
			const VkImage& image, 
			VkImageMemoryBarrier& imgMemoryBarrier, 
			VkPipelineStageFlags& sourceStage, 
			VkPipelineStageFlags& destinationStage)
		{
            imgMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imgMemoryBarrier.oldLayout = oldLayout;
            imgMemoryBarrier.newLayout = newLayout;
            imgMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imgMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imgMemoryBarrier.image = image;
            imgMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imgMemoryBarrier.subresourceRange.baseMipLevel = 0;
            imgMemoryBarrier.subresourceRange.baseArrayLayer = 0;

            if (isCubemap)
                imgMemoryBarrier.subresourceRange.layerCount = 6;
            else
                imgMemoryBarrier.subresourceRange.layerCount = 1;

            imgMemoryBarrier.subresourceRange.levelCount = mipLevels;

            imgMemoryBarrier.srcAccessMask = 0;
            imgMemoryBarrier.dstAccessMask = 0;

            if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
                newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                ) {

                imgMemoryBarrier.srcAccessMask = 0;
                imgMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

            }
            else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
                newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                ) {

                imgMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                imgMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
                destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

            }
            else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
                newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                ) {

                imgMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imgMemoryBarrier.dstAccessMask = (
                    VK_ACCESS_HOST_WRITE_BIT |
                    VK_ACCESS_TRANSFER_WRITE_BIT
                    );

                sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
                destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

            }
            else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                ) {

                imgMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imgMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

            }
            else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
                newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                ) {

                imgMemoryBarrier.srcAccessMask = 0;
                imgMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
                destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

            }
            else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
                newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                ) {

                imgMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                imgMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

                sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
                destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

            }
            else
                throw std::invalid_argument("Unsupported layout transition!");
		}
	}
}






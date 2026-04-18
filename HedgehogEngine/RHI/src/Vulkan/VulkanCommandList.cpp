#include "VulkanCommandList.hpp"

#include "VulkanBuffer.hpp"
#include "VulkanDescriptor.hpp"
#include "VulkanDevice.hpp"
#include "VulkanFramebuffer.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanRenderPass.hpp"
#include "VulkanTexture.hpp"
#include "VulkanTypes.hpp"

#include <cassert>

namespace RHI
{

VulkanCommandList::VulkanCommandList(VulkanDevice& device)
    : m_Device(device)
{
    VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.commandPool        = m_Device.GetCommandPool();
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkResult result = vkAllocateCommandBuffers(
        m_Device.GetHandle(), &allocInfo, &m_CommandBuffer);
    assert(result == VK_SUCCESS && "Failed to allocate VkCommandBuffer.");
}

VulkanCommandList::~VulkanCommandList()
{
    if (m_CommandBuffer != VK_NULL_HANDLE)
        vkFreeCommandBuffers(
            m_Device.GetHandle(), m_Device.GetCommandPool(), 1, &m_CommandBuffer);
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void VulkanCommandList::Reset()
{
    vkResetCommandBuffer(m_CommandBuffer, 0);
}

void VulkanCommandList::Begin(bool oneTimeSubmit)
{
    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    if (oneTimeSubmit)
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkResult result = vkBeginCommandBuffer(m_CommandBuffer, &beginInfo);
    assert(result == VK_SUCCESS && "vkBeginCommandBuffer failed.");
}

void VulkanCommandList::End()
{
    VkResult result = vkEndCommandBuffer(m_CommandBuffer);
    assert(result == VK_SUCCESS && "vkEndCommandBuffer failed.");
}

// ── Render pass ───────────────────────────────────────────────────────────────

void VulkanCommandList::BeginRenderPass(
    const IRHIRenderPass&         renderPass,
    const IRHIFramebuffer&         framebuffer,
    const std::vector<ClearValue>& clearValues)
{
    const auto& vkPass = static_cast<const VulkanRenderPass&>(renderPass);
    const auto& vkFb   = static_cast<const VulkanFramebuffer&>(framebuffer);

    std::vector<VkClearValue> vkClearValues;
    vkClearValues.reserve(clearValues.size());
    for (const auto& cv : clearValues)
    {
        VkClearValue vkCv{};
        if (cv.m_IsDepth)
            vkCv.depthStencil = { cv.m_DepthStencil.m_Depth, cv.m_DepthStencil.m_Stencil };
        else
            vkCv.color = { cv.m_Color.m_R, cv.m_Color.m_G, cv.m_Color.m_B, cv.m_Color.m_A };
        vkClearValues.push_back(vkCv);
    }

    VkRenderPassBeginInfo beginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    beginInfo.renderPass               = vkPass.GetHandle();
    beginInfo.framebuffer              = vkFb.GetHandle();
    beginInfo.renderArea.offset        = { 0, 0 };
    beginInfo.renderArea.extent        = { vkFb.GetWidth(), vkFb.GetHeight() };
    beginInfo.clearValueCount          = static_cast<uint32_t>(vkClearValues.size());
    beginInfo.pClearValues             = vkClearValues.data();

    vkCmdBeginRenderPass(m_CommandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanCommandList::EndRenderPass()
{
    vkCmdEndRenderPass(m_CommandBuffer);
}

// ── Pipeline state ────────────────────────────────────────────────────────────

void VulkanCommandList::BindPipeline(const IRHIPipeline& pipeline)
{
    const auto& vkPipeline = static_cast<const VulkanPipeline&>(pipeline);
    vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline.GetHandle());
}

void VulkanCommandList::SetViewport(const Viewport& viewport)
{
    VkViewport vkVp{};
    vkVp.x        = viewport.m_X;
    vkVp.y        = viewport.m_Y;
    vkVp.width    = viewport.m_Width;
    vkVp.height   = viewport.m_Height;
    vkVp.minDepth = viewport.m_MinDepth;
    vkVp.maxDepth = viewport.m_MaxDepth;
    vkCmdSetViewport(m_CommandBuffer, 0, 1, &vkVp);
}

void VulkanCommandList::SetScissor(const Scissor& scissor)
{
    VkRect2D rect{};
    rect.offset = { scissor.m_X, scissor.m_Y };
    rect.extent = { scissor.m_Width, scissor.m_Height };
    vkCmdSetScissor(m_CommandBuffer, 0, 1, &rect);
}

// ── Resource binding ──────────────────────────────────────────────────────────

void VulkanCommandList::BindVertexBuffers(
    uint32_t                        firstBinding,
    const std::vector<IRHIBuffer*>& buffers,
    const std::vector<size_t>&      offsets)
{
    assert(buffers.size() == offsets.size());

    std::vector<VkBuffer>     vkBuffers;
    std::vector<VkDeviceSize> vkOffsets;
    vkBuffers.reserve(buffers.size());
    vkOffsets.reserve(offsets.size());

    for (size_t i = 0; i < buffers.size(); ++i)
    {
        vkBuffers.push_back(static_cast<const VulkanBuffer&>(*buffers[i]).GetHandle());
        vkOffsets.push_back(static_cast<VkDeviceSize>(offsets[i]));
    }

    vkCmdBindVertexBuffers(m_CommandBuffer, firstBinding,
                           static_cast<uint32_t>(vkBuffers.size()),
                           vkBuffers.data(), vkOffsets.data());
}

void VulkanCommandList::BindIndexBuffer(
    const IRHIBuffer& buffer, IndexType type, size_t offset)
{
    const auto& vkBuf = static_cast<const VulkanBuffer&>(buffer);
    vkCmdBindIndexBuffer(m_CommandBuffer, vkBuf.GetHandle(),
                         static_cast<VkDeviceSize>(offset),
                         VulkanTypes::ToVkIndexType(type));
}

void VulkanCommandList::BindDescriptorSet(
    const IRHIPipeline& pipeline, uint32_t setIndex, const IRHIDescriptorSet& set)
{
    const auto& vkPipeline = static_cast<const VulkanPipeline&>(pipeline);
    const auto& vkSet      = static_cast<const VulkanDescriptorSet&>(set);

    VkDescriptorSet descriptorSet = vkSet.GetHandle();
    vkCmdBindDescriptorSets(
        m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        vkPipeline.GetLayout(),
        setIndex, 1, &descriptorSet,
        0, nullptr);
}

void VulkanCommandList::PushConstants(
    const IRHIPipeline& pipeline,
    ShaderStage         stages,
    uint32_t            offset,
    uint32_t            size,
    const void*         data)
{
    const auto& vkPipeline = static_cast<const VulkanPipeline&>(pipeline);
    vkCmdPushConstants(
        m_CommandBuffer, vkPipeline.GetLayout(),
        VulkanTypes::ToVkShaderStage(stages),
        offset, size, data);
}

// ── Draw calls ────────────────────────────────────────────────────────────────

void VulkanCommandList::Draw(
    uint32_t vertexCount, uint32_t instanceCount,
    uint32_t firstVertex, uint32_t firstInstance)
{
    vkCmdDraw(m_CommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void VulkanCommandList::DrawIndexed(
    uint32_t indexCount,   uint32_t instanceCount,
    uint32_t firstIndex,   int32_t  vertexOffset,
    uint32_t firstInstance)
{
    vkCmdDrawIndexed(m_CommandBuffer, indexCount, instanceCount,
                     firstIndex, vertexOffset, firstInstance);
}

// ── Barriers / copies ─────────────────────────────────────────────────────────

void VulkanCommandList::TransitionTexture(
    IRHITexture& texture, ImageLayout oldLayout, ImageLayout newLayout)
{
    auto& vkTex = static_cast<VulkanTexture&>(texture);

    VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    barrier.oldLayout                       = VulkanTypes::ToVkLayout(oldLayout);
    barrier.newLayout                       = VulkanTypes::ToVkLayout(newLayout);
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = vkTex.GetHandle();
    barrier.subresourceRange.aspectMask     = VulkanTypes::GetAspectMask(vkTex.GetFormat());
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    VulkanTypes::FillBarrierStages(barrier, oldLayout, newLayout);

    VkDependencyInfo depInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers    = &barrier;

    vkCmdPipelineBarrier2(m_CommandBuffer, &depInfo);
}

void VulkanCommandList::CopyBufferToBuffer(
    const IRHIBuffer& src, IRHIBuffer& dst,
    size_t srcOffset, size_t dstOffset, size_t size)
{
    const auto& vkSrc = static_cast<const VulkanBuffer&>(src);
    const auto& vkDst = static_cast<const VulkanBuffer&>(dst);

    VkBufferCopy region{};
    region.srcOffset = static_cast<VkDeviceSize>(srcOffset);
    region.dstOffset = static_cast<VkDeviceSize>(dstOffset);
    region.size      = static_cast<VkDeviceSize>(size);

    vkCmdCopyBuffer(m_CommandBuffer, vkSrc.GetHandle(), vkDst.GetHandle(), 1, &region);
}

void VulkanCommandList::CopyBufferToTexture(const IRHIBuffer& src, IRHITexture& dst)
{
    const auto& vkSrc = static_cast<const VulkanBuffer&>(src);
    auto&       vkDst = static_cast<VulkanTexture&>(dst);

    VkBufferImageCopy region{};
    region.bufferOffset                    = 0;
    region.bufferRowLength                 = 0;
    region.bufferImageHeight               = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset                     = { 0, 0, 0 };
    region.imageExtent                     = { vkDst.GetWidth(), vkDst.GetHeight(), 1 };

    vkCmdCopyBufferToImage(
        m_CommandBuffer,
        vkSrc.GetHandle(),
        vkDst.GetHandle(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region);
}

void VulkanCommandList::CopyTextureToTexture(const IRHITexture& src, IRHITexture& dst)
{
    const auto& vkSrc = static_cast<const VulkanTexture&>(src);
    auto&       vkDst = static_cast<VulkanTexture&>(dst);

    VkImageBlit2 region{ VK_STRUCTURE_TYPE_IMAGE_BLIT_2 };

    region.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.mipLevel       = 0;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.layerCount     = 1;
    region.srcOffsets[0]                 = { 0, 0, 0 };
    region.srcOffsets[1]                 = { static_cast<int32_t>(vkSrc.GetWidth()),
                                             static_cast<int32_t>(vkSrc.GetHeight()), 1 };

    region.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.mipLevel       = 0;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.layerCount     = 1;
    region.dstOffsets[0]                 = { 0, 0, 0 };
    region.dstOffsets[1]                 = { static_cast<int32_t>(vkDst.GetWidth()),
                                             static_cast<int32_t>(vkDst.GetHeight()), 1 };

    VkBlitImageInfo2 blitInfo{ VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2 };
    blitInfo.srcImage       = vkSrc.GetHandle();
    blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blitInfo.dstImage       = vkDst.GetHandle();
    blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blitInfo.regionCount    = 1;
    blitInfo.pRegions       = &region;
    blitInfo.filter         = VK_FILTER_LINEAR;

    vkCmdBlitImage2(m_CommandBuffer, &blitInfo);
}

} // namespace RHI

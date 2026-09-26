#include "VulkanCommandList.hpp"

#include "VulkanBuffer.hpp"
#include "VulkanDescriptor.hpp"
#include "VulkanDevice.hpp"
#include "VulkanPipeline.hpp"
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

// ── Dynamic rendering ────────────────────────────────────────────────────────

namespace
{
    VkClearValue ToVkClearValue(const ClearValue& cv)
    {
        VkClearValue vkCv{};
        if (cv.IsDepth)
            vkCv.depthStencil = { cv.DepthStencil.Depth, cv.DepthStencil.Stencil };
        else
            vkCv.color = { cv.Color.R, cv.Color.G, cv.Color.B, cv.Color.A };
        return vkCv;
    }

    VkRenderingAttachmentInfo ToVkRenderingAttachmentInfo(const RenderingAttachment& attachment,
                                                            VkImageLayout               layout)
    {
        const auto& vkTex = static_cast<const VulkanTexture&>(*attachment.Texture);

        VkRenderingAttachmentInfo info{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        info.imageView   = vkTex.GetViewHandle();
        info.imageLayout = layout;
        info.loadOp       = VulkanTypes::ToVkLoadOp(attachment.LoadOp);
        info.storeOp      = VulkanTypes::ToVkStoreOp(attachment.StoreOp);
        info.clearValue  = ToVkClearValue(attachment.Clear);
        return info;
    }
}

void VulkanCommandList::BeginRendering(const RenderingInfo& renderingInfo)
{
    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    colorAttachments.reserve(renderingInfo.ColorAttachments.size());
    for (const auto& attachment : renderingInfo.ColorAttachments)
    {
        assert(attachment.Texture && "RenderingAttachment::Texture must not be null.");
        colorAttachments.push_back(
            ToVkRenderingAttachmentInfo(attachment, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    }

    VkRenderingAttachmentInfo depthAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    if (renderingInfo.DepthAttachment)
    {
        assert(renderingInfo.DepthAttachment->Texture && "RenderingAttachment::Texture must not be null.");
        depthAttachment = ToVkRenderingAttachmentInfo(
            *renderingInfo.DepthAttachment, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    VkRenderingInfo vkInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
    vkInfo.renderArea.offset    = { 0, 0 };
    vkInfo.renderArea.extent    = { renderingInfo.Width, renderingInfo.Height };
    vkInfo.layerCount           = 1;
    vkInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
    vkInfo.pColorAttachments    = colorAttachments.data();
    vkInfo.pDepthAttachment     = renderingInfo.DepthAttachment ? &depthAttachment : nullptr;

    vkCmdBeginRendering(m_CommandBuffer, &vkInfo);
}

void VulkanCommandList::EndRendering()
{
    vkCmdEndRendering(m_CommandBuffer);
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
    vkVp.x        = viewport.X;
    vkVp.y        = viewport.Y;
    vkVp.width    = viewport.Width;
    vkVp.height   = viewport.Height;
    vkVp.minDepth = viewport.MinDepth;
    vkVp.maxDepth = viewport.MaxDepth;
    vkCmdSetViewport(m_CommandBuffer, 0, 1, &vkVp);
}

void VulkanCommandList::SetScissor(const Scissor& scissor)
{
    VkRect2D rect{};
    rect.offset = { scissor.X, scissor.Y };
    rect.extent = { scissor.Width, scissor.Height };
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

void VulkanCommandList::Barrier(
    std::span<const TextureBarrier> textureBarriers,
    std::span<const BufferBarrier>  bufferBarriers)
{
    std::vector<VkImageMemoryBarrier2> imageBarriers;
    imageBarriers.reserve(textureBarriers.size());
    for (const auto& tb : textureBarriers)
    {
        assert(tb.Texture && "TextureBarrier::Texture must not be null.");
        const auto& vkTex = static_cast<const VulkanTexture&>(*tb.Texture);

        const auto before = VulkanTypes::ToVkResourceStateInfo(tb.Before);
        const auto after  = VulkanTypes::ToVkResourceStateInfo(tb.After);

        VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        barrier.srcStageMask        = before.Stage;
        barrier.srcAccessMask       = before.Access;
        barrier.dstStageMask        = after.Stage;
        barrier.dstAccessMask       = after.Access;
        barrier.oldLayout           = before.Layout;
        barrier.newLayout           = after.Layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = vkTex.GetHandle();
        barrier.subresourceRange    = VulkanTypes::ToVkSubresourceRange(
            tb.Range, VulkanTypes::GetAspectMask(vkTex.GetFormat()));
        imageBarriers.push_back(barrier);
    }

    std::vector<VkBufferMemoryBarrier2> bufferMemoryBarriers;
    bufferMemoryBarriers.reserve(bufferBarriers.size());
    for (const auto& bb : bufferBarriers)
    {
        assert(bb.Buffer && "BufferBarrier::Buffer must not be null.");
        const auto& vkBuf = static_cast<const VulkanBuffer&>(*bb.Buffer);

        const auto before = VulkanTypes::ToVkResourceStateInfo(bb.Before);
        const auto after  = VulkanTypes::ToVkResourceStateInfo(bb.After);

        VkBufferMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
        barrier.srcStageMask        = before.Stage;
        barrier.srcAccessMask       = before.Access;
        barrier.dstStageMask        = after.Stage;
        barrier.dstAccessMask       = after.Access;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer              = vkBuf.GetHandle();
        barrier.offset              = static_cast<VkDeviceSize>(bb.Offset);
        barrier.size                = bb.Size == RHI::WHOLE_BUFFER_SIZE
                                         ? VK_WHOLE_SIZE : static_cast<VkDeviceSize>(bb.Size);
        bufferMemoryBarriers.push_back(barrier);
    }

    if (imageBarriers.empty() && bufferMemoryBarriers.empty())
        return;

    VkDependencyInfo depInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    depInfo.imageMemoryBarrierCount  = static_cast<uint32_t>(imageBarriers.size());
    depInfo.pImageMemoryBarriers     = imageBarriers.data();
    depInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(bufferMemoryBarriers.size());
    depInfo.pBufferMemoryBarriers    = bufferMemoryBarriers.data();

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

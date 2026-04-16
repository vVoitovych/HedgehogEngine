#pragma once

#include "../RHI/IRHICommandList.hpp"

#include "volk.h"

namespace RHI
{

class VulkanDevice;

class VulkanCommandList final : public IRHICommandList
{
public:
    explicit VulkanCommandList(VulkanDevice& device);
    ~VulkanCommandList() override;

    VulkanCommandList(const VulkanCommandList&)            = delete;
    VulkanCommandList& operator=(const VulkanCommandList&) = delete;
    VulkanCommandList(VulkanCommandList&&)                 = delete;
    VulkanCommandList& operator=(VulkanCommandList&&)      = delete;

    // ── IRHICommandList ───────────────────────────────────────────────────────

    void Reset() override;
    void Begin(bool oneTimeSubmit = false) override;
    void End()   override;

    void BeginRenderPass(const IRHIRenderPass&         renderPass,
                         const IRHIFramebuffer&         framebuffer,
                         const std::vector<ClearValue>& clearValues) override;
    void EndRenderPass() override;

    void BindPipeline(const IRHIPipeline& pipeline)       override;
    void SetViewport(const Viewport& viewport)             override;
    void SetScissor(const Scissor& scissor)                override;

    void BindVertexBuffers(uint32_t                         firstBinding,
                           const std::vector<IRHIBuffer*>&  buffers,
                           const std::vector<size_t>&       offsets) override;

    void BindIndexBuffer(const IRHIBuffer& buffer,
                         IndexType         type,
                         size_t            offset = 0) override;

    void BindDescriptorSet(const IRHIPipeline&      pipeline,
                           uint32_t                 setIndex,
                           const IRHIDescriptorSet& set) override;

    void PushConstants(const IRHIPipeline& pipeline,
                        ShaderStage         stages,
                        uint32_t            offset,
                        uint32_t            size,
                        const void*         data) override;

    void Draw(uint32_t vertexCount,   uint32_t instanceCount,
              uint32_t firstVertex,   uint32_t firstInstance) override;

    void DrawIndexed(uint32_t indexCount,   uint32_t instanceCount,
                     uint32_t firstIndex,   int32_t  vertexOffset,
                     uint32_t firstInstance) override;

    void TransitionTexture(IRHITexture& texture,
                           ImageLayout  oldLayout,
                           ImageLayout  newLayout) override;

    void CopyBufferToBuffer(const IRHIBuffer& src,
                            IRHIBuffer&       dst,
                            size_t            srcOffset,
                            size_t            dstOffset,
                            size_t            size) override;

    void CopyBufferToTexture(const IRHIBuffer& src, IRHITexture& dst) override;

    // Internal accessor
    VkCommandBuffer GetHandle() const { return m_CommandBuffer; }

private:
    VulkanDevice&   m_Device;
    VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
};

} // namespace RHI

#pragma once

#include "RHITypes.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace RHI
{

class IRHIBuffer;
class IRHITexture;
class IRHIPipeline;
class IRHIDescriptorSet;
class IRHIRenderPass;
class IRHIFramebuffer;

// One attachment for BeginRendering (VK_KHR_dynamic_rendering / Vulkan 1.3 core): a texture
// view plus the load/store/clear behaviour that RenderPassDesc+ClearValue split across a
// render pass object and a separate clear-values vector. The caller transitions Texture into
// the right layout (ColorAttachment / DepthStencilAttachment) beforehand, same as it already
// does before BeginRenderPass.
struct RenderingAttachment
{
    const IRHITexture* Texture = nullptr;
    RHI::LoadOp         LoadOp  = RHI::LoadOp::Load;
    RHI::StoreOp         StoreOp = RHI::StoreOp::Store;
    ClearValue            Clear   = {};
};

struct RenderingInfo
{
    std::vector<RenderingAttachment>   ColorAttachments;
    std::optional<RenderingAttachment> DepthAttachment;
    uint32_t                           Width  = 0;
    uint32_t                           Height = 0;
};

class IRHICommandList
{
public:
    virtual ~IRHICommandList() = default;

    IRHICommandList(const IRHICommandList&)            = delete;
    IRHICommandList& operator=(const IRHICommandList&) = delete;
    IRHICommandList(IRHICommandList&&)                 = delete;
    IRHICommandList& operator=(IRHICommandList&&)      = delete;

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    // Reset the underlying command buffer so it can be re-recorded.
    virtual void Reset() = 0;

    // Begin recording.
    // oneTimeSubmit: hint that this recording will be submitted exactly once
    // (enables driver optimisations; use for immediate/upload commands).
    virtual void Begin(bool oneTimeSubmit = false) = 0;

    virtual void End() = 0;

    // ── Render pass ───────────────────────────────────────────────────────────

    virtual void BeginRenderPass(const IRHIRenderPass&        renderPass,
                                 const IRHIFramebuffer&        framebuffer,
                                 const std::vector<ClearValue>& clearValues) = 0;

    virtual void EndRenderPass() = 0;

    // ── Dynamic rendering (VK_KHR_dynamic_rendering / Vulkan 1.3 core) ─────────

    // Alternative to BeginRenderPass/EndRenderPass that needs no IRHIRenderPass or
    // IRHIFramebuffer object — attachments are texture views passed directly. A pipeline
    // bound inside must have been created with matching GraphicsPipelineDesc attachment
    // formats (RenderPass left null), not a render-pass-compatible one.
    virtual void BeginRendering(const RenderingInfo& renderingInfo) = 0;

    virtual void EndRendering() = 0;

    // ── Pipeline state ────────────────────────────────────────────────────────

    virtual void BindPipeline(const IRHIPipeline& pipeline) = 0;

    virtual void SetViewport(const Viewport& viewport) = 0;
    virtual void SetScissor(const Scissor& scissor)    = 0;

    // ── Resource binding ──────────────────────────────────────────────────────

    // buffers.size() == offsets.size()
    virtual void BindVertexBuffers(uint32_t                          firstBinding,
                                   const std::vector<IRHIBuffer*>&  buffers,
                                   const std::vector<size_t>&       offsets) = 0;

    virtual void BindIndexBuffer(const IRHIBuffer& buffer,
                                 IndexType         type,
                                 size_t            offset = 0) = 0;

    virtual void BindDescriptorSet(const IRHIPipeline&      pipeline,
                                   uint32_t                 setIndex,
                                   const IRHIDescriptorSet& set) = 0;

    virtual void PushConstants(const IRHIPipeline& pipeline,
                                ShaderStage         stages,
                                uint32_t            offset,
                                uint32_t            size,
                                const void*         data) = 0;

    // ── Draw calls ────────────────────────────────────────────────────────────

    virtual void Draw(uint32_t vertexCount,
                      uint32_t instanceCount = 1,
                      uint32_t firstVertex   = 0,
                      uint32_t firstInstance = 0) = 0;

    virtual void DrawIndexed(uint32_t indexCount,
                              uint32_t instanceCount = 1,
                              uint32_t firstIndex    = 0,
                              int32_t  vertexOffset  = 0,
                              uint32_t firstInstance = 0) = 0;

    // ── Transfer / barrier ────────────────────────────────────────────────────

    // Insert a pipeline barrier transitioning the texture layout.
    virtual void TransitionTexture(IRHITexture& texture,
                                   ImageLayout  oldLayout,
                                   ImageLayout  newLayout) = 0;

    virtual void CopyBufferToBuffer(const IRHIBuffer& src,
                                    IRHIBuffer&       dst,
                                    size_t            srcOffset,
                                    size_t            dstOffset,
                                    size_t            size) = 0;

    // Copies the entire buffer content into the texture (assumes staging → GPU).
    virtual void CopyBufferToTexture(const IRHIBuffer& src,
                                     IRHITexture&      dst) = 0;

    // Blits src into dst using linear filtering (handles size mismatch).
    // Both textures must already be in TransferSrc / TransferDst layout respectively.
    virtual void CopyTextureToTexture(const IRHITexture& src,
                                      IRHITexture&       dst) = 0;

protected:
    IRHICommandList() = default;
};

} // namespace RHI

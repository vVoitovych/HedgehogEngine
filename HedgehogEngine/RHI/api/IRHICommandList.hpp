#pragma once

#include "RHITypes.hpp"

#include <cstddef>
#include <vector>

namespace RHI
{

class IRHIBuffer;
class IRHITexture;
class IRHIPipeline;
class IRHIDescriptorSet;
class IRHIRenderPass;
class IRHIFramebuffer;

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

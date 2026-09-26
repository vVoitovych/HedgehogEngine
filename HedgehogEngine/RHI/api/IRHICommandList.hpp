#pragma once

#include "RHITypes.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace RHI
{

class IRHIBuffer;
class IRHITexture;
class IRHIPipeline;
class IRHIDescriptorSet;

// One attachment for BeginRendering (Vulkan 1.3 dynamic rendering): a texture view plus its
// load/store/clear behaviour. The caller transitions Texture into RenderTarget / DepthWrite with
// Barrier() beforehand.
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

// One entry in a Barrier() call. Before/After are states, never raw layouts — the backend
// derives the layout, pipeline stage and access mask from each. Range defaulted covers the
// whole resource.
struct TextureBarrier
{
    IRHITexture*             Texture = nullptr;
    RHI::ResourceState        Before  = RHI::ResourceState::Undefined;
    RHI::ResourceState        After   = RHI::ResourceState::Undefined;
    TextureSubresourceRange    Range   = {};
};

struct BufferBarrier
{
    IRHIBuffer*        Buffer = nullptr;
    RHI::ResourceState Before  = RHI::ResourceState::Undefined;
    RHI::ResourceState After   = RHI::ResourceState::Undefined;
    size_t             Offset  = 0;
    size_t             Size    = WHOLE_BUFFER_SIZE;
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

    // ── Dynamic rendering (VK_KHR_dynamic_rendering / Vulkan 1.3 core) ─────────

    // Attachments are texture views passed directly; there are no render-pass or framebuffer
    // objects. A pipeline bound inside must have been created with matching GraphicsPipelineDesc
    // attachment formats.
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

    // Batches any number of texture and buffer transitions into a single synchronization2
    // vkCmdPipelineBarrier2 call, access-based (ResourceState) rather than raw layouts. The only
    // transition mechanism (RENDERING.md section 5.3).
    virtual void Barrier(std::span<const TextureBarrier> textureBarriers,
                         std::span<const BufferBarrier>  bufferBarriers) = 0;

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

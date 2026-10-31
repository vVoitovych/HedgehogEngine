#pragma once

#include "GpuPassTiming.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHIQueryPool.hpp"

#include <span>
#include <vector>

namespace Renderer
{
    // A command list that records into another one unchanged and, while started, writes a GPU
    // timestamp after every draw, barrier, copy, blit and mipmap generation, remembering what each
    // was, and at the start and end of every scope (as ScopeBegin and ScopeEnd entries). Timestamp
    // first is written at Start, so command i runs from first + i to first + i + 1.
    // Once capacity - 1 commands are timed, later ones are still recorded, only not timed.
    class TimedCommandList final : public RHI::IRHICommandList
    {
    public:
        // Resets queries first .. first + capacity - 1 on target (outside rendering) and writes the
        // first; the commands of the last capture are dropped.
        void Start(RHI::IRHICommandList& target, RHI::IRHIQueryPool& pool, uint32_t first, uint32_t capacity);
        // The pass the commands recorded from now on belong to; a name that outlives the capture.
        void SetPass(const char* pass) { m_Pass = pass; }
        // Stops recording into the target; the commands stay until the next Start or Clear.
        void Finish() { m_Target = nullptr; }
        void Clear() { m_Commands.clear(); }

        // The commands timed, in order; their GpuMilliseconds are 0 until filled from the timestamps.
        [[nodiscard]] std::span<const GpuCommandTiming> GetCommands() const { return m_Commands; }
        // Whether commands went untimed because the capacity ran out.
        [[nodiscard]] bool IsTruncated() const { return m_Truncated; }

        void Reset() override { m_Target->Reset(); }
        void Begin(bool oneTimeSubmit) override { m_Target->Begin(oneTimeSubmit); }
        void End() override { m_Target->End(); }

        void BeginRendering(const RHI::RenderingInfo& renderingInfo) override { m_Target->BeginRendering(renderingInfo); }
        void EndRendering() override { m_Target->EndRendering(); }

        void BindPipeline(const RHI::IRHIPipeline& pipeline) override { m_Target->BindPipeline(pipeline); }
        void SetViewport(const RHI::Viewport& viewport) override { m_Target->SetViewport(viewport); }
        void SetScissor(const RHI::Scissor& scissor) override { m_Target->SetScissor(scissor); }

        void BindVertexBuffers(uint32_t firstBinding, const std::vector<RHI::IRHIBuffer*>& buffers,
                               const std::vector<size_t>& offsets) override
        {
            m_Target->BindVertexBuffers(firstBinding, buffers, offsets);
        }
        void BindIndexBuffer(const RHI::IRHIBuffer& buffer, RHI::IndexType type, size_t offset) override
        {
            m_Target->BindIndexBuffer(buffer, type, offset);
        }
        void BindDescriptorSet(const RHI::IRHIPipeline& pipeline, uint32_t setIndex, const RHI::IRHIDescriptorSet& set) override
        {
            m_Target->BindDescriptorSet(pipeline, setIndex, set);
        }
        void PushConstants(const RHI::IRHIPipeline& pipeline, RHI::ShaderStage stages, uint32_t offset, uint32_t size,
                           const void* data) override
        {
            m_Target->PushConstants(pipeline, stages, offset, size, data);
        }

        void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset,
                         uint32_t firstInstance) override;
        void Barrier(std::span<const RHI::TextureBarrier> textureBarriers,
                     std::span<const RHI::BufferBarrier>  bufferBarriers) override;
        void CopyBufferToBuffer(const RHI::IRHIBuffer& src, RHI::IRHIBuffer& dst, size_t srcOffset, size_t dstOffset,
                                size_t size) override;
        void CopyBufferToTexture(const RHI::IRHIBuffer& src, RHI::IRHITexture& dst) override;
        void CopyBufferToTexture(const RHI::IRHIBuffer& src, RHI::IRHITexture& dst, const RHI::TextureRegion& region) override;
        void GenerateMipmaps(RHI::IRHITexture& texture) override;
        void CopyTextureToTexture(const RHI::IRHITexture& src, RHI::IRHITexture& dst) override;

        void ResetQueries(RHI::IRHIQueryPool& pool, uint32_t first, uint32_t count) override
        {
            m_Target->ResetQueries(pool, first, count);
        }
        void WriteTimestamp(RHI::IRHIQueryPool& pool, uint32_t index) override { m_Target->WriteTimestamp(pool, index); }

        // An EndScope with no scope open is ignored.
        void BeginScope(const char* name) override;
        void EndScope() override;

        // Commands recorded straight into the target's native handle (ImGui's) go untimed.
        [[nodiscard]] const RHI::IRHICommandList& GetRecordingList() const override { return m_Target->GetRecordingList(); }

    private:
        void Stamp(GpuCommandKind kind, uint32_t count, uint32_t instances, const char* scope = nullptr);

        RHI::IRHICommandList*         m_Target   = nullptr;
        RHI::IRHIQueryPool*           m_Pool     = nullptr;
        uint32_t                      m_First    = 0;
        uint32_t                      m_Capacity = 0;
        const char*                   m_Pass     = "";
        bool                          m_Truncated = false;
        uint32_t                      m_ScopeDepth = 0;
        std::vector<GpuCommandTiming> m_Commands; // reused frame to frame
    };
}

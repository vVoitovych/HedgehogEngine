#pragma once

// Minimal fakes satisfying IRHIDevice/IRHITexture/IRHIBuffer/IRHICommandList so ResourcePool and
// RenderGraphRuntime are testable with no Vulkan SDK, no GPU, no instance — every method these
// interfaces declare has zero Vulkan dependency in its own signature, so a plain C++ class can
// implement them. Only CreateTexture and Barrier are meaningful; everything else exists purely
// to satisfy the interface and is never expected to be called by these tests.

#include "RHI/api/IRHIDevice.hpp"

#include <cassert>
#include <vector>

namespace RGTest
{
    class TestTexture final : public RHI::IRHITexture
    {
    public:
        explicit TestTexture(const RHI::TextureDesc& desc) : m_Desc(desc) {}

        uint32_t                  GetWidth()  const override { return m_Desc.Width; }
        uint32_t                  GetHeight() const override { return m_Desc.Height; }
        RHI::Format                GetFormat() const override { return m_Desc.Format; }
        const RHI::TextureDesc&      GetDesc()   const override { return m_Desc; }

    private:
        RHI::TextureDesc m_Desc;
    };

    class TestBuffer final : public RHI::IRHIBuffer
    {
    public:
        explicit TestBuffer(size_t size) : m_Size(size) {}

        size_t GetSize() const override { return m_Size; }
        void   CopyData(const void*, size_t, size_t) override {}
        void*  Map() override { return nullptr; }
        void   Unmap() override {}

    private:
        size_t m_Size;
    };

    class TestDevice final : public RHI::IRHIDevice
    {
    public:
        std::unique_ptr<RHI::IRHIBuffer> CreateBuffer(size_t size, RHI::BufferUsage, RHI::MemoryUsage) const override
        {
            return std::make_unique<TestBuffer>(size);
        }

        std::unique_ptr<RHI::IRHITexture> CreateTexture(const RHI::TextureDesc& desc) const override
        {
            ++m_TexturesCreated;
            return std::make_unique<TestTexture>(desc);
        }

        std::unique_ptr<RHI::IRHITextureView> CreateTextureView(
            const RHI::IRHITexture&, const RHI::TextureSubresourceRange&) const override
        {
            assert(false && "not exercised by these tests");
            return nullptr;
        }

        std::unique_ptr<RHI::IRHISampler> CreateSampler(const RHI::SamplerDesc&) const override
        {
            assert(false && "not exercised by these tests");
            return nullptr;
        }

        std::unique_ptr<RHI::IRHIShader> CreateShader(std::span<const std::byte>, RHI::ShaderStage) const override
        {
            assert(false && "not exercised by these tests");
            return nullptr;
        }

        std::unique_ptr<RHI::IRHIDescriptorSetLayout> CreateDescriptorSetLayout(
            const std::vector<RHI::DescriptorBinding>&) const override
        {
            assert(false && "not exercised by these tests");
            return nullptr;
        }

        std::unique_ptr<RHI::IRHIDescriptorPool> CreateDescriptorPool(
            uint32_t, const std::vector<RHI::PoolSize>&) const override
        {
            assert(false && "not exercised by these tests");
            return nullptr;
        }

        std::unique_ptr<RHI::IRHIDescriptorSet> AllocateDescriptorSet(
            const RHI::IRHIDescriptorPool&, const RHI::IRHIDescriptorSetLayout&) const override
        {
            assert(false && "not exercised by these tests");
            return nullptr;
        }

        std::unique_ptr<RHI::IRHIRenderPass> CreateRenderPass(const RHI::RenderPassDesc&) const override
        {
            assert(false && "not exercised by these tests");
            return nullptr;
        }

        std::unique_ptr<RHI::IRHIFramebuffer> CreateFramebuffer(const RHI::FramebufferDesc&) const override
        {
            assert(false && "not exercised by these tests");
            return nullptr;
        }

        std::unique_ptr<RHI::IRHIPipeline> CreateGraphicsPipeline(const RHI::GraphicsPipelineDesc&) const override
        {
            assert(false && "not exercised by these tests");
            return nullptr;
        }

        std::unique_ptr<RHI::IRHICommandList> CreateCommandList() const override
        {
            assert(false && "not exercised by these tests");
            return nullptr;
        }

        std::unique_ptr<RHI::IRHISwapchain> CreateSwapchain(uint32_t, uint32_t) const override
        {
            assert(false && "not exercised by these tests");
            return nullptr;
        }

        std::unique_ptr<RHI::IRHIFence> CreateFence(bool) const override
        {
            assert(false && "not exercised by these tests");
            return nullptr;
        }

        std::unique_ptr<RHI::IRHISemaphore> CreateSemaphore() const override
        {
            assert(false && "not exercised by these tests");
            return nullptr;
        }

        void SubmitCommandList(const RHI::IRHICommandList&, const std::vector<RHI::IRHISemaphore*>&,
                               const std::vector<RHI::IRHISemaphore*>&, RHI::IRHIFence*) override
        {
            assert(false && "not exercised by these tests");
        }

        void ExecuteImmediately(std::function<void(RHI::IRHICommandList&)>) const override
        {
            assert(false && "not exercised by these tests");
        }

        void WaitIdle() const override {}

        RHI::Format GetPreferredDepthFormat() const override { return RHI::Format::D32Float; }

        std::unique_ptr<RHI::IRHIGuiBackend> CreateGuiBackend(const RHI::GuiBackendDesc&) const override
        {
            assert(false && "not exercised by these tests");
            return nullptr;
        }

        mutable int m_TexturesCreated = 0;
    };

    // Records every Barrier() call it receives instead of issuing anything — what
    // RenderGraphRuntime::Execute tests inspect to confirm resolved barriers/execute order.
    class RecordingCommandList final : public RHI::IRHICommandList
    {
    public:
        struct BarrierCall
        {
            std::vector<RHI::TextureBarrier> TextureBarriers;
            std::vector<RHI::BufferBarrier>  BufferBarriers;
        };

        void Reset() override {}
        void Begin(bool) override {}
        void End() override {}

        void BeginRenderPass(const RHI::IRHIRenderPass&, const RHI::IRHIFramebuffer&,
                             const std::vector<RHI::ClearValue>&) override {}
        void EndRenderPass() override {}

        void BeginRendering(const RHI::RenderingInfo&) override {}
        void EndRendering() override {}

        void Barrier(std::span<const RHI::TextureBarrier> textureBarriers,
                    std::span<const RHI::BufferBarrier>  bufferBarriers) override
        {
            BarrierCall call;
            call.TextureBarriers.assign(textureBarriers.begin(), textureBarriers.end());
            call.BufferBarriers.assign(bufferBarriers.begin(), bufferBarriers.end());
            Calls.push_back(std::move(call));
        }

        void BindPipeline(const RHI::IRHIPipeline&) override {}
        void SetViewport(const RHI::Viewport&) override {}
        void SetScissor(const RHI::Scissor&) override {}

        void BindVertexBuffers(uint32_t, const std::vector<RHI::IRHIBuffer*>&,
                               const std::vector<size_t>&) override {}
        void BindIndexBuffer(const RHI::IRHIBuffer&, RHI::IndexType, size_t) override {}
        void BindDescriptorSet(const RHI::IRHIPipeline&, uint32_t, const RHI::IRHIDescriptorSet&) override {}
        void PushConstants(const RHI::IRHIPipeline&, RHI::ShaderStage, uint32_t, uint32_t, const void*) override {}

        void Draw(uint32_t, uint32_t, uint32_t, uint32_t) override {}
        void DrawIndexed(uint32_t, uint32_t, uint32_t, int32_t, uint32_t) override {}

        void TransitionTexture(RHI::IRHITexture&, RHI::ImageLayout, RHI::ImageLayout) override {}
        void CopyBufferToBuffer(const RHI::IRHIBuffer&, RHI::IRHIBuffer&, size_t, size_t, size_t) override {}
        void CopyBufferToTexture(const RHI::IRHIBuffer&, RHI::IRHITexture&) override {}
        void CopyTextureToTexture(const RHI::IRHITexture&, RHI::IRHITexture&) override {}

        std::vector<BarrierCall> Calls;
    };
}

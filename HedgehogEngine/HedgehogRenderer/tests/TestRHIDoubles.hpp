#pragma once

// Minimal fakes satisfying IRHIDevice/IRHITexture/IRHIBuffer/IRHICommandList so ResourcePool and
// RenderGraphRuntime are testable with no Vulkan SDK, no GPU, no instance — every method these
// interfaces declare has zero Vulkan dependency in its own signature, so a plain C++ class can
// implement them. Only CreateTexture and Barrier are meaningful; everything else exists purely
// to satisfy the interface and is never expected to be called by these tests.

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace RGTest
{
    class TestTexture final : public RHI::IRHITexture
    {
    public:
        // destroyedCounter, when given, is incremented on destruction, so tests can check exactly
        // when a texture is released (RenderTargetRegistry's fence-deferred destruction).
        explicit TestTexture(const RHI::TextureDesc& desc, int* destroyedCounter = nullptr)
            : m_Desc(desc), m_DestroyedCounter(destroyedCounter) {}
        ~TestTexture() override
        {
            if (m_DestroyedCounter)
                ++*m_DestroyedCounter;
        }

        uint32_t                  GetWidth()  const override { return m_Desc.Width; }
        uint32_t                  GetHeight() const override { return m_Desc.Height; }
        RHI::Format                GetFormat() const override { return m_Desc.Format; }
        const RHI::TextureDesc&      GetDesc()   const override { return m_Desc; }

    private:
        RHI::TextureDesc m_Desc;
        int*             m_DestroyedCounter = nullptr;
    };

    // A swapchain with a settable extent. Resize() only records the new size, as a real one would
    // after recreating its images; nothing is acquired or presented.
    class TestSwapchain final : public RHI::IRHISwapchain
    {
    public:
        TestSwapchain(uint32_t width, uint32_t height) { Resize(width, height); }

        uint32_t     GetImageCount() const override { return static_cast<uint32_t>(m_Images.size()); }
        RHI::Format  GetFormat()     const override { return RHI::Format::B8G8R8A8Srgb; }
        uint32_t     GetWidth()      const override { return m_Width; }
        uint32_t     GetHeight()     const override { return m_Height; }
        RHI::IRHITexture& GetTexture(uint32_t index) override { return *m_Images.at(index); }
        uint32_t     AcquireNextImage(RHI::IRHISemaphore&) override { return 0; }
        void         Present(uint32_t, RHI::IRHISemaphore&) override {}

        void Resize(uint32_t width, uint32_t height) override
        {
            m_Width  = width;
            m_Height = height;
            m_Images.clear();
            for (int i = 0; i < 2; ++i)
            {
                RHI::TextureDesc desc;
                desc.Width  = width;
                desc.Height = height;
                desc.Format = RHI::Format::B8G8R8A8Srgb;
                m_Images.push_back(std::make_unique<TestTexture>(desc));
            }
        }

    private:
        uint32_t                                      m_Width  = 0;
        uint32_t                                      m_Height = 0;
        std::vector<std::unique_ptr<RHI::IRHITexture>> m_Images;
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
            return std::make_unique<TestTexture>(desc, &m_TexturesDestroyed);
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

        mutable int m_TexturesCreated   = 0;
        mutable int m_TexturesDestroyed = 0;
    };

    // Records every Barrier() call it receives instead of issuing anything — what
    // RenderGraphRuntime::Execute tests inspect to confirm resolved barriers/execute order — and
    // counts the rendering, binding and draw calls pass execute bodies make.
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

        void BeginRendering(const RHI::RenderingInfo& info) override
        {
            Renderings.push_back(info);
            Commands.push_back("begin");
        }
        void EndRendering() override { ++EndRenderingCount; }

        void Barrier(std::span<const RHI::TextureBarrier> textureBarriers,
                    std::span<const RHI::BufferBarrier>  bufferBarriers) override
        {
            BarrierCall call;
            call.TextureBarriers.assign(textureBarriers.begin(), textureBarriers.end());
            call.BufferBarriers.assign(bufferBarriers.begin(), bufferBarriers.end());
            Calls.push_back(std::move(call));
        }

        void BindPipeline(const RHI::IRHIPipeline& pipeline) override
        {
            BoundPipelines.push_back(&pipeline);
            Commands.push_back("pipeline");
        }
        void SetViewport(const RHI::Viewport& viewport) override { Viewports.push_back(viewport); }
        void SetScissor(const RHI::Scissor& scissor) override { Scissors.push_back(scissor); }

        void BindVertexBuffers(uint32_t, const std::vector<RHI::IRHIBuffer*>& buffers,
                               const std::vector<size_t>&) override
        {
            Commands.push_back("vertex " + std::to_string(buffers.size()));
            BoundVertexBuffers.emplace_back(buffers.begin(), buffers.end());
        }
        void BindIndexBuffer(const RHI::IRHIBuffer&, RHI::IndexType, size_t) override { Commands.push_back("index"); }
        void BindDescriptorSet(const RHI::IRHIPipeline&, uint32_t setIndex, const RHI::IRHIDescriptorSet& set) override
        {
            ++DescriptorSetBinds;
            BoundSetIndices.push_back(setIndex);
            BoundSets.push_back(&set);
            Commands.push_back("set " + std::to_string(setIndex));
        }
        void PushConstants(const RHI::IRHIPipeline&, RHI::ShaderStage, uint32_t, uint32_t size, const void* data) override
        {
            Commands.push_back("push " + std::to_string(size));
            const auto* bytes = static_cast<const unsigned char*>(data);
            PushedData.emplace_back(bytes, bytes + size);
            if (size > 64) // the skinned pipelines' palette offset, after the model matrix
            {
                uint32_t offset = 0;
                std::memcpy(&offset, static_cast<const char*>(data) + 64, sizeof(offset));
                PushedPaletteOffsets.push_back(offset);
            }
        }

        void Draw(uint32_t vertexCount, uint32_t, uint32_t, uint32_t) override { DrawnVertexCounts.push_back(vertexCount); }
        void DrawIndexed(uint32_t indexCount, uint32_t, uint32_t, int32_t, uint32_t) override
        {
            DrawnIndexCounts.push_back(indexCount);
            Commands.push_back("draw " + std::to_string(indexCount));
        }

        void CopyBufferToBuffer(const RHI::IRHIBuffer&, RHI::IRHIBuffer&, size_t, size_t, size_t) override {}
        void CopyBufferToTexture(const RHI::IRHIBuffer&, RHI::IRHITexture&) override {}
        void CopyBufferToTexture(const RHI::IRHIBuffer&, RHI::IRHITexture& dst, const RHI::TextureRegion& region) override
        {
            RegionCopies.push_back({ &dst, region });
            Commands.push_back("copy " + std::to_string(region.MipLevel) + " " + std::to_string(region.ArrayLayer));
        }
        void GenerateMipmaps(RHI::IRHITexture& texture) override
        {
            MipmapGenerations.push_back(&texture);
            Commands.push_back("mipmaps");
        }
        void CopyTextureToTexture(const RHI::IRHITexture&, RHI::IRHITexture&) override {}

        struct RegionCopy
        {
            const RHI::IRHITexture* Texture = nullptr;
            RHI::TextureRegion      Region;
        };
        std::vector<RegionCopy>              RegionCopies;
        std::vector<const RHI::IRHITexture*> MipmapGenerations;

        std::vector<BarrierCall>         Calls;
        std::vector<RHI::RenderingInfo>  Renderings;
        int                              EndRenderingCount  = 0;
        std::vector<RHI::Viewport>       Viewports;
        std::vector<RHI::Scissor>        Scissors;
        int                              DescriptorSetBinds = 0;
        std::vector<uint32_t>            BoundSetIndices;
        std::vector<uint32_t>            DrawnIndexCounts;
        std::vector<uint32_t>            DrawnVertexCounts;

        // Every rendering begun, pipeline, vertex/index buffer, descriptor set, push constant,
        // indexed draw, region copy and mipmap generation, in order, as "begin", "pipeline",
        // "vertex <count>", "index", "set <index>", "push <bytes>", "draw <index count>",
        // "copy <mip> <layer>" and "mipmaps".
        std::vector<std::string>                   Commands;
        std::vector<const RHI::IRHIPipeline*>      BoundPipelines;
        std::vector<const RHI::IRHIDescriptorSet*> BoundSets;
        std::vector<uint32_t>                      PushedPaletteOffsets;
        // Every vertex buffer list bound, in order.
        std::vector<std::vector<const RHI::IRHIBuffer*>> BoundVertexBuffers;
        // Every push constant's bytes, in order.
        std::vector<std::vector<unsigned char>>    PushedData;
    };
}

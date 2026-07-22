#pragma once

#include "ResourceManager/ResourceManager.hpp"

#include "HedgehogCommon/api/Frame/FrameData.hpp"

#include <HedgehogMath/api/Matrix.hpp>

#include <array>
#include <memory>
#include <vector>

namespace RHI
{
    class IRHIDevice;
    class IRHICommandList;
    class IRHIRenderPass;
    class IRHIPipeline;
    class IRHIFramebuffer;
    class IRHIDescriptorSetLayout;
    class IRHIDescriptorPool;
    class IRHIDescriptorSet;
    class IRHIBuffer;
}

namespace FS
{
    class FileSystemManager;
}

namespace Renderer
{
    class ResourceManager;

    class DepthPrePass
    {
    public:
        DepthPrePass(RHI::IRHIDevice& device, const ResourceManager& resourceManager,
                     const FS::FileSystemManager& fileSystem);
        ~DepthPrePass();

        // Renders the depth prepass for one view (target) from the given camera.
        void Render(RenderTargetId target,
                    const HedgehogEngine::CameraData& camera,
                    const HedgehogEngine::DrawBucket&  opaque,
                    const ResourceManager& resourceManager,
                    RHI::IRHICommandList& cmd, uint32_t frameIndex);
        void Cleanup(RHI::IRHIDevice& device);

        void ResizeResources(RenderTargetId target, RHI::IRHIDevice& device,
                             const ResourceManager& resourceManager);

    private:
        struct DepthPrepassFrameUniform
        {
            alignas(16) HM::Matrix4x4 ViewProj;
        };

        void CreateFramebuffer(RenderTargetId target, RHI::IRHIDevice& device,
                               const ResourceManager& resourceManager);

    private:
        std::unique_ptr<RHI::IRHIRenderPass>          m_RenderPass;
        std::unique_ptr<RHI::IRHIPipeline>            m_Pipeline;
        std::unique_ptr<RHI::IRHIDescriptorSetLayout> m_FrameLayout;
        std::unique_ptr<RHI::IRHIDescriptorPool>      m_FramePool;

        // Per-target framebuffers and per-target × per-frame uniforms/descriptor sets, so the
        // scene and game views can be recorded in the same frame without clobbering each other.
        std::array<std::unique_ptr<RHI::IRHIFramebuffer>, kRenderTargetCount> m_FrameBuffers;
        std::array<std::vector<std::unique_ptr<RHI::IRHIBuffer>>, kRenderTargetCount>        m_FrameUniforms;
        std::array<std::vector<std::unique_ptr<RHI::IRHIDescriptorSet>>, kRenderTargetCount> m_FrameSets;
    };

}

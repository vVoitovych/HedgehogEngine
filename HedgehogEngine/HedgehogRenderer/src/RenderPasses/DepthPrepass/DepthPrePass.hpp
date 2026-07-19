#pragma once

#include "RenderGraph/IRenderPass.hpp"
#include "RenderGraph/RenderGraphTypes.hpp"

#include <HedgehogMath/api/Matrix.hpp>

#include <memory>
#include <vector>

namespace RHI
{
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
    class DepthPrePass : public IRenderPass
    {
    public:
        DepthPrePass(RHI::IRHIDevice& device, const FS::FileSystemManager& fileSystem);
        ~DepthPrePass() override;

        const char* GetName() const override { return "DepthPrePass"; }

        void Setup(RenderGraphBuilder& builder) override;
        void CreateFramebuffers(RHI::IRHIDevice& device, RenderGraph& graph) override;
        void Execute(RenderGraphContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;

    private:
        struct DepthPrepassFrameUniform
        {
            alignas(16) HM::Matrix4x4 m_ViewProj;
        };

    private:
        ResourceHandle m_SceneDepthHandle = INVALID_RESOURCE_HANDLE;

        std::unique_ptr<RHI::IRHIRenderPass>         m_RenderPass;
        std::unique_ptr<RHI::IRHIFramebuffer>         m_FrameBuffer;
        std::unique_ptr<RHI::IRHIPipeline>            m_Pipeline;

        std::unique_ptr<RHI::IRHIDescriptorSetLayout> m_FrameLayout;
        std::unique_ptr<RHI::IRHIDescriptorPool>      m_FramePool;

        std::vector<std::unique_ptr<RHI::IRHIBuffer>>        m_FrameUniforms;
        std::vector<std::unique_ptr<RHI::IRHIDescriptorSet>> m_FrameSets;
    };

}

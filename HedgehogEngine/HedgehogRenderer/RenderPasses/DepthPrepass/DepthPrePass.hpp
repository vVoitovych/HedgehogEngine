#pragma once

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

namespace Context
{
    class Context;
}

namespace Renderer
{
    class ResourceManager;

    class DepthPrePass
    {
    public:
        DepthPrePass(const Context::Context& context, const ResourceManager& resourceManager);
        ~DepthPrePass();

        void Render(Context::Context& context, const ResourceManager& resourceManager);
        void Cleanup(const Context::Context& context);

        void ResizeResources(const Context::Context& context, const ResourceManager& resourceManager);

    private:
        struct DepthPrepassFrameUniform
        {
            alignas(16) HM::Matrix4x4 m_ViewProj;
        };

    private:
        std::unique_ptr<RHI::IRHIRenderPass>         m_RenderPass;
        std::unique_ptr<RHI::IRHIFramebuffer>         m_FrameBuffer;
        std::unique_ptr<RHI::IRHIPipeline>            m_Pipeline;

        std::unique_ptr<RHI::IRHIDescriptorSetLayout> m_FrameLayout;
        std::unique_ptr<RHI::IRHIDescriptorPool>      m_FramePool;

        std::vector<std::unique_ptr<RHI::IRHIBuffer>>        m_FrameUniforms;
        std::vector<std::unique_ptr<RHI::IRHIDescriptorSet>> m_FrameSets;
    };

}

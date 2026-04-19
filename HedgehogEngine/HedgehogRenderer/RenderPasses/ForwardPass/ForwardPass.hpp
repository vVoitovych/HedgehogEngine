#pragma once

#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogContext/Containers/LightContainer/Light.hpp"
#include "HedgehogMath/api/Matrix.hpp"

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

    class ForwardPass
    {
    public:
        ForwardPass(const Context::Context& context, const ResourceManager& resourceManager);
        ~ForwardPass();

        void Render(Context::Context& context, const ResourceManager& resourceManager);
        void Cleanup(const Context::Context& context);

        void ResizeResources(const Context::Context& context, const ResourceManager& resourceManager);

    private:
        struct ForwardPassFrameUniform
        {
            alignas(16) HM::Matrix4x4  m_View;
            alignas(16) HM::Matrix4x4  m_ViewProj;
            alignas(16) HM::Vector3    m_EyePosition;
            alignas(16) Context::Light m_Lights[MAX_LIGHTS_COUNT];
            size_t                     m_LightCount;
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

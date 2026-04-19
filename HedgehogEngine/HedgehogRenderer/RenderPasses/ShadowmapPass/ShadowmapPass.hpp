#pragma once

#include <HedgehogMath/api/Matrix.hpp>

#include <array>
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

    class ShadowmapPass
    {
    public:
        ShadowmapPass(const Context::Context& context, const ResourceManager& resourceManager);
        ~ShadowmapPass();

        void Render(Context::Context& context, const ResourceManager& resourceManager);
        void Cleanup(const Context::Context& context);

        void UpdateData(const Context::Context& context);
        void UpdateResources(const Context::Context& context, const ResourceManager& resourceManager);

    private:
        void UpdateFrameBuffer(const Context::Context& context, const ResourceManager& resourceManager);
        void UpdateViewports(const Context::Context& context);
        void UpdateShadowmapMatrices(const Context::Context& context);

    private:
        struct ShadowCascadeUniform
        {
            alignas(16) HM::Matrix4x4 m_ShadowMatrix;
        };

        struct ShadowViewport
        {
            float m_X      = 0.0f;
            float m_Y      = 0.0f;
            float m_Width  = 0.0f;
            float m_Height = 0.0f;
        };

        static constexpr uint32_t MaxShadowCascades = 4;

        std::array<HM::Matrix4x4, MaxShadowCascades> m_ShadowmapMatrices;
        std::vector<std::vector<ShadowViewport>>      m_ShadowViewports;

        std::unique_ptr<RHI::IRHIRenderPass>         m_RenderPass;
        std::unique_ptr<RHI::IRHIFramebuffer>         m_FrameBuffer;
        std::unique_ptr<RHI::IRHIPipeline>            m_Pipeline;

        std::unique_ptr<RHI::IRHIDescriptorSetLayout> m_ShadowmapLayout;
        std::unique_ptr<RHI::IRHIDescriptorPool>      m_ShadowmapPool;

        // [frame][cascade]
        std::vector<std::vector<std::unique_ptr<RHI::IRHIBuffer>>>        m_ShadowmapUniforms;
        std::vector<std::vector<std::unique_ptr<RHI::IRHIDescriptorSet>>> m_ShadowmapSets;
    };

}

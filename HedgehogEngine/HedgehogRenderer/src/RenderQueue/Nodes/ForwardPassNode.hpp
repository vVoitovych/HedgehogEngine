#pragma once

#include "../IRenderNode.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace RHI
{
    class IRHIDevice;
    class IRHIRenderPass;
    class IRHIPipeline;
    class IRHIFramebuffer;
    class IRHIDescriptorSetLayout;
    class IRHIDescriptorPool;
    class IRHIDescriptorSet;
    class IRHIBuffer;
}

namespace HedgehogEngine
{
    struct LightData;
}

namespace Renderer
{
    class ResourceManager;

    class ForwardPassNode final : public IRenderNode
    {
    public:
        ForwardPassNode(RHI::IRHIDevice& device, const ResourceManager& resourceManager);
        ~ForwardPassNode() override;

        void Render(RenderContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;

    private:
        void RebuildFramebufferIfNeeded(RHI::IRHIDevice& device, const ResourceManager& rm);

        struct GpuLight
        {
            alignas(16) HM::Vector3 m_Position;
            alignas(16) HM::Vector3 m_Direction;
            alignas(16) HM::Vector3 m_Color;
            alignas(16) HM::Vector4 m_Data; // (type, intensity, radius, cos(coneAngle))
        };

        struct FrameUniform
        {
            alignas(16) HM::Matrix4x4 m_View;
            alignas(16) HM::Matrix4x4 m_ViewProj;
            alignas(16) HM::Vector3   m_EyePosition;
            alignas(16) GpuLight      m_Lights[MAX_LIGHTS_COUNT];
            size_t                    m_LightCount;
        };

        static GpuLight ToGpuLight(const HedgehogEngine::LightData& ld);

        std::unique_ptr<RHI::IRHIRenderPass>          m_RenderPass;
        std::unique_ptr<RHI::IRHIFramebuffer>          m_FrameBuffer;
        std::unique_ptr<RHI::IRHIPipeline>             m_Pipeline;
        std::unique_ptr<RHI::IRHIDescriptorSetLayout>  m_FrameLayout;
        std::unique_ptr<RHI::IRHIDescriptorPool>       m_FramePool;
        std::vector<std::unique_ptr<RHI::IRHIBuffer>>        m_FrameUniforms;
        std::vector<std::unique_ptr<RHI::IRHIDescriptorSet>> m_FrameSets;

        uint32_t m_CachedWidth  = 0;
        uint32_t m_CachedHeight = 0;
    };

} // namespace Renderer

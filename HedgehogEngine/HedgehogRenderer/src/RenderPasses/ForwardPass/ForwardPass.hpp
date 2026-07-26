#pragma once

#include "RenderGraph/IRenderPass.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogCommon/api/Frame/FrameData.hpp"

#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <memory>
#include <vector>

namespace RHI
{
    class IRHIDevice;
    class IRHIFramebuffer;
    class IRHIDescriptorPool;
    class IRHIDescriptorSet;
    class IRHIBuffer;
}

namespace Renderer
{
    class ForwardPassResources;
    struct PassInitContext;

    // View-stage pass: lit forward geometry into this view's own viewColor target. One instance
    // per view — see DepthPrePass.hpp for the shared-resources/per-instance split this relies on.
    class ForwardPass : public IRenderPass
    {
    public:
        explicit ForwardPass(const PassInitContext& init);
        ~ForwardPass() override;

        const char* GetName() const override { return "ForwardPass"; }

        void Setup(RenderGraphBuilder& builder) override;
        void CreateFramebuffers(RHI::IRHIDevice& device, RenderGraph& graph) override;
        void Execute(RenderGraphContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;

    private:
        // GPU-layout light struct; alignas matches std140/std430 UBO packing expected by the shader.
        struct GpuLight
        {
            alignas(16) HM::Vector3 Position;
            alignas(16) HM::Vector3 Direction;
            alignas(16) HM::Vector3 Color;
            alignas(16) HM::Vector4 Data;  // (type, intensity, radius, cos(coneAngle))
        };

        struct ForwardPassFrameUniform
        {
            alignas(16) HM::Matrix4x4 View;
            alignas(16) HM::Matrix4x4 ViewProj;
            alignas(16) HM::Vector3   EyePosition;
            alignas(16) GpuLight      Lights[HedgehogEngine::MAX_LIGHTS_COUNT];
            size_t                    LightCount;
        };

        static GpuLight ToGpuLight(const HedgehogEngine::LightData& fd);

    private:
        // Shared, immutable half (render pass, pipeline, descriptor-set layouts) — see
        // PassResourceCache.
        std::shared_ptr<const ForwardPassResources> m_Resources;

        std::unique_ptr<RHI::IRHIDescriptorPool> m_FramePool;
        std::unique_ptr<RHI::IRHIFramebuffer>    m_FrameBuffer;

        std::vector<std::unique_ptr<RHI::IRHIBuffer>>        m_FrameUniforms; // [frame]
        std::vector<std::unique_ptr<RHI::IRHIDescriptorSet>> m_FrameSets;     // [frame]
    };
}

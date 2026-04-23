#pragma once

#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Vector.hpp"

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

namespace FD
{
    struct FrameData;
    struct LightData;
}

namespace Renderer
{
    class ResourceManager;

    class ForwardPass
    {
    public:
        ForwardPass(RHI::IRHIDevice& device, const ResourceManager& resourceManager);
        ~ForwardPass();

        void Render(const FD::FrameData& frame, const ResourceManager& resourceManager,
                    RHI::IRHICommandList& cmd, uint32_t frameIndex);
        void Cleanup(RHI::IRHIDevice& device);

        void ResizeResources(RHI::IRHIDevice& device, const ResourceManager& resourceManager);

    private:
        // GPU-layout light struct; alignas matches std140/std430 UBO packing expected by the shader.
        struct GpuLight
        {
            alignas(16) HM::Vector3 m_Position;
            alignas(16) HM::Vector3 m_Direction;
            alignas(16) HM::Vector3 m_Color;
            alignas(16) HM::Vector4 m_Data;  // (type, intensity, radius, cos(coneAngle))
        };

        struct ForwardPassFrameUniform
        {
            alignas(16) HM::Matrix4x4 m_View;
            alignas(16) HM::Matrix4x4 m_ViewProj;
            alignas(16) HM::Vector3   m_EyePosition;
            alignas(16) GpuLight      m_Lights[MAX_LIGHTS_COUNT];
            size_t                    m_LightCount;
        };

        static GpuLight ToGpuLight(const FD::LightData& fd);

    private:
        std::unique_ptr<RHI::IRHIRenderPass>  m_RenderPass;
        std::unique_ptr<RHI::IRHIFramebuffer> m_FrameBuffer;
        std::unique_ptr<RHI::IRHIPipeline>    m_Pipeline;

        std::unique_ptr<RHI::IRHIDescriptorSetLayout> m_FrameLayout;
        std::unique_ptr<RHI::IRHIDescriptorPool>      m_FramePool;

        std::vector<std::unique_ptr<RHI::IRHIBuffer>>        m_FrameUniforms;
        std::vector<std::unique_ptr<RHI::IRHIDescriptorSet>> m_FrameSets;
    };
}

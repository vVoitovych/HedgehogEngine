#pragma once

#include "RenderGraph/IRenderPass.hpp"
#include "RenderGraph/RenderGraphTypes.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Vector.hpp"

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

namespace HedgehogEngine
{
    struct LightData;
}

namespace FS
{
    class FileSystemManager;
}

namespace HR
{
    class ResourceRegistry;
}

namespace Renderer
{
    class ForwardPass : public IRenderPass
    {
    public:
        ForwardPass(RHI::IRHIDevice& device, HR::ResourceRegistry& resourceRegistry,
                    const FS::FileSystemManager& fileSystem);
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
            alignas(16) GpuLight      m_Lights[HedgehogEngine::MAX_LIGHTS_COUNT];
            size_t                    m_LightCount;
        };

        static GpuLight ToGpuLight(const HedgehogEngine::LightData& fd);

    private:
        ResourceHandle m_SceneDepthHandle = INVALID_RESOURCE_HANDLE;
        ResourceHandle m_SceneColorHandle = INVALID_RESOURCE_HANDLE;

        std::unique_ptr<RHI::IRHIRenderPass>  m_RenderPass;
        std::unique_ptr<RHI::IRHIFramebuffer> m_FrameBuffer;
        std::unique_ptr<RHI::IRHIPipeline>    m_Pipeline;

        std::unique_ptr<RHI::IRHIDescriptorSetLayout> m_FrameLayout;
        std::unique_ptr<RHI::IRHIDescriptorPool>      m_FramePool;
        std::unique_ptr<RHI::IRHIDescriptorSetLayout> m_MaterialLayout;

        std::vector<std::unique_ptr<RHI::IRHIBuffer>>        m_FrameUniforms;
        std::vector<std::unique_ptr<RHI::IRHIDescriptorSet>> m_FrameSets;
    };
}

#pragma once

#include "ResourceManager/ResourceManager.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogCommon/api/Frame/FrameData.hpp"

#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Vector.hpp"

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

    class ForwardPass
    {
    public:
        ForwardPass(RHI::IRHIDevice& device, ResourceManager& resourceManager,
                    const FS::FileSystemManager& fileSystem);
        ~ForwardPass();

        // Renders the lit geometry pass for one view (target) from the given camera. Pass an empty
        // draw bucket to produce a clear-only frame (e.g. game view with no primary camera).
        void Render(RenderTargetId target,
                    const HedgehogEngine::CameraData&          camera,
                    const HedgehogEngine::DrawBucket&          opaque,
                    const std::vector<HedgehogEngine::LightData>& lights,
                    const ResourceManager& resourceManager,
                    RHI::IRHICommandList& cmd, uint32_t frameIndex);
        void Cleanup(RHI::IRHIDevice& device);

        void ResizeResources(RenderTargetId target, RHI::IRHIDevice& device,
                             const ResourceManager& resourceManager);

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

        void CreateFramebuffer(RenderTargetId target, RHI::IRHIDevice& device,
                               const ResourceManager& resourceManager);

    private:
        std::unique_ptr<RHI::IRHIRenderPass>  m_RenderPass;
        std::unique_ptr<RHI::IRHIPipeline>    m_Pipeline;

        std::unique_ptr<RHI::IRHIDescriptorSetLayout> m_FrameLayout;
        std::unique_ptr<RHI::IRHIDescriptorPool>      m_FramePool;
        std::unique_ptr<RHI::IRHIDescriptorSetLayout> m_MaterialLayout;

        // Per-target framebuffers and per-target × per-frame uniforms/descriptor sets, so the
        // scene and game views can be recorded in the same frame without clobbering each other.
        std::array<std::unique_ptr<RHI::IRHIFramebuffer>, kRenderTargetCount> m_FrameBuffers;
        std::array<std::vector<std::unique_ptr<RHI::IRHIBuffer>>, kRenderTargetCount>        m_FrameUniforms;
        std::array<std::vector<std::unique_ptr<RHI::IRHIDescriptorSet>>, kRenderTargetCount> m_FrameSets;
    };
}

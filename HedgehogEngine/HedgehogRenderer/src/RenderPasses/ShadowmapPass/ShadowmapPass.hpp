#pragma once

#include <HedgehogMath/api/Matrix.hpp>

#include <array>
#include <memory>
#include <optional>
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

namespace HedgehogEngine
{
    struct FrameData;
    struct CameraData;
}

namespace HedgehogSettings
{
    class Settings;
}

namespace Renderer
{
    class ResourceManager;

    class ShadowmapPass
    {
    public:
        ShadowmapPass(RHI::IRHIDevice& device, const HedgehogSettings::Settings& settings,
                      const ResourceManager& resourceManager);
        ~ShadowmapPass();

        void Render(const HedgehogEngine::FrameData& frame, const ResourceManager& resourceManager,
                    RHI::IRHICommandList& cmd, uint32_t frameIndex);
        void Cleanup(RHI::IRHIDevice& device);

        void UpdateData(const HedgehogEngine::FrameData& frame, uint32_t frameIndex,
                        const HedgehogSettings::Settings& settings);
        void UpdateResources(RHI::IRHIDevice& device, const HedgehogSettings::Settings& settings,
                             const ResourceManager& resourceManager);

    private:
        void UpdateFrameBuffer(RHI::IRHIDevice& device, const ResourceManager& resourceManager);
        void UpdateViewports(const HedgehogSettings::Settings& settings);
        void UpdateShadowmapMatrices(const HedgehogEngine::CameraData& camera,
                                     const HedgehogSettings::Settings& settings,
                                     const std::optional<HM::Vector3>& shadowLightDir);

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

        uint32_t m_CascadesCount = 1;
        uint32_t m_ShadowmapSize = 1024;

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

#pragma once

#include "RenderGraph/IRenderPass.hpp"
#include "RenderGraph/RenderGraphTypes.hpp"

#include <HedgehogMath/api/Matrix.hpp>

#include <array>
#include <memory>
#include <optional>
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
    struct CameraData;
}

namespace HedgehogSettings
{
    class Settings;
}

namespace FS
{
    class FileSystemManager;
}

namespace Renderer
{
    class ShadowmapPass : public IRenderPass
    {
    public:
        ShadowmapPass(RHI::IRHIDevice& device, const HedgehogSettings::Settings& settings,
                      const FS::FileSystemManager& fileSystem);
        ~ShadowmapPass() override;

        const char* GetName() const override { return "ShadowmapPass"; }

        void Setup(RenderGraphBuilder& builder) override;
        void CreateFramebuffers(RHI::IRHIDevice& device, RenderGraph& graph) override;
        void Update(const RenderGraphContext& ctx) override;
        void Execute(RenderGraphContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;

    private:
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

        const HedgehogSettings::Settings& m_Settings;

        uint32_t m_CascadesCount = 1;
        uint32_t m_ShadowmapSize = 1024;

        std::array<HM::Matrix4x4, MaxShadowCascades> m_ShadowmapMatrices;
        std::vector<std::vector<ShadowViewport>>      m_ShadowViewports;

        ResourceHandle m_ShadowDepthHandle = INVALID_RESOURCE_HANDLE;

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

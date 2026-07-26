#pragma once

#include "RenderGraph/IRenderPass.hpp"

#include <HedgehogMath/api/Matrix.hpp>

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace RHI
{
    class IRHIDevice;
    class IRHIFramebuffer;
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
    class ShadowmapPassResources;
    struct PassInitContext;

    // Frame-graph pass: renders cascaded shadow depth once per frame, fit to the currently-main
    // view's camera (RenderGraphContext::View, repointed by Renderer to the main view while the
    // frame graph executes — see workflow/current-plan.md, "Main-view camera for frame-stage
    // passes") — not per-view; every view samples the same shared shadow map. No pass currently
    // reads shadowDepth, so Setup() declares it write-only. The shadow map is now a graph-owned
    // Fixed-size transient (shared/shadowDepth) rather than a ResourceManager-owned texture —
    // UpdateShadowSettings() drives its resize via RenderGraph::SetFixedSize + Invalidate.
    class ShadowmapPass : public IRenderPass
    {
    public:
        explicit ShadowmapPass(const PassInitContext& init);
        ~ShadowmapPass() override;

        const char* GetName() const override { return "ShadowmapPass"; }

        void Setup(RenderGraphBuilder& builder) override;
        void CreateFramebuffers(RHI::IRHIDevice& device, RenderGraph& graph) override;
        void Update(const RenderGraphContext& ctx) override;
        void Execute(RenderGraphContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;

        // Called by RenderPipeline::NotifySettingsDirty, itself called by Renderer once per frame
        // only when settings.IsDirty() (the outer dirty flag — see Renderer::DrawFrame). Checks the
        // shadowmap-specific IsDirty()/CleanDirtyState() pair itself, matching the two-level
        // dirty-check this replaces from ResourceManager::ResizeSettingsDependentResources +
        // ShadowmapPass::UpdateResources.
        void OnSettingsDirty(RenderGraph& graph, RHI::IRHIDevice& device,
                             const HedgehogSettings::Settings& settings) override;

    private:
        void UpdateViewports(const HedgehogSettings::Settings& settings);
        void UpdateShadowmapMatrices(const HedgehogEngine::CameraData& camera,
                                     const HedgehogSettings::Settings& settings,
                                     const std::optional<HM::Vector3>& shadowLightDir);

    private:
        struct ShadowCascadeUniform
        {
            alignas(16) HM::Matrix4x4 ShadowMatrix;
        };

        struct ShadowViewport
        {
            float X      = 0.0f;
            float Y      = 0.0f;
            float Width  = 0.0f;
            float Height = 0.0f;
        };

        static constexpr uint32_t MaxShadowCascades = 4;

        uint32_t m_CascadesCount = 1;
        uint32_t m_ShadowmapSize = 1024;

        std::array<HM::Matrix4x4, MaxShadowCascades> m_ShadowmapMatrices;
        std::vector<std::vector<ShadowViewport>>      m_ShadowViewports;

        // Shared, immutable half (render pass, pipeline, per-cascade descriptor-set layout) —
        // see PassResourceCache.
        std::shared_ptr<const ShadowmapPassResources> m_Resources;

        std::unique_ptr<RHI::IRHIFramebuffer>    m_FrameBuffer;
        std::unique_ptr<RHI::IRHIDescriptorPool> m_ShadowmapPool;

        // [frame][cascade]
        std::vector<std::vector<std::unique_ptr<RHI::IRHIBuffer>>>        m_ShadowmapUniforms;
        std::vector<std::vector<std::unique_ptr<RHI::IRHIDescriptorSet>>> m_ShadowmapSets;
    };

}

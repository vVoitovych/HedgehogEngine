#pragma once

#include "RenderGraph/IRenderPass.hpp"

#include "HedgehogCommon/api/Frame/FrameData.hpp"
#include "HedgehogCommon/api/RendererSettings.hpp"

#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace RHI
{
    class IRHIDevice;
    class IRHIFramebuffer;
    class IRHIBuffer;
}

namespace Renderer
{
    class GizmoPassResources;
    struct PassInitContext;

    // View-stage pass: draws selection/light gizmo lines over this view's own viewColor target
    // (LoadOp::Load — composited on top of ForwardPass's output within the same view graph). One
    // instance per view that wants gizmos (scene view only — Renderer decides which views get a
    // GizmoPass at CreateView time). Reads RenderGraphContext::View for the selected-entity
    // matrix and camera; a game build's view simply never has this pass registered.
    class GizmoPass : public IRenderPass
    {
    public:
        explicit GizmoPass(const PassInitContext& init);
        ~GizmoPass() override;

        const char* GetName() const override { return "GizmoPass"; }

        void Setup(RenderGraphBuilder& builder) override;
        void CreateFramebuffers(RHI::IRHIDevice& device, RenderGraph& graph) override;
        void Execute(RenderGraphContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;

    private:
        // Tightly packed (24 bytes) to match PositionColor.vdes: vec3 position + vec3 colour.
        struct GizmoVertex { float px, py, pz, cx, cy, cz; };

        void BuildLines(const std::vector<HedgehogEngine::LightData>& lights,
                        const std::optional<HM::Matrix4x4>&           selected);

    private:
        // Shared, immutable half (render pass, pipeline) — see PassResourceCache.
        std::shared_ptr<const GizmoPassResources> m_Resources;

        std::unique_ptr<RHI::IRHIFramebuffer> m_FrameBuffer;

        std::array<std::unique_ptr<RHI::IRHIBuffer>, HedgehogEngine::MAX_FRAMES_IN_FLIGHT> m_VertexBuffers;
        std::vector<GizmoVertex> m_Lines; // reused CPU scratch, rebuilt each frame
    };
}

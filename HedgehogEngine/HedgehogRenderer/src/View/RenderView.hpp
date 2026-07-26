#pragma once

#include "RenderPipeline/RenderPipeline.hpp"
#include "RenderGraph/RenderGraphTypes.hpp"

#include <string>

namespace Renderer
{
    class RenderResourceLedger;

    // One rendered viewport: an editor Scene panel, a Game panel, or (in a non-editor build) the
    // single game view. Composes a RenderPipeline (its own RenderGraph, scoped to this view's
    // name, plus the pass instances that graph drives) with the state that varies per view: the
    // application-supplied camera/gizmo payload (ViewFrameData), desired vs. currently-compiled
    // size, and whether it's enabled this frame. See workflow/current-plan.md, "Per-view resource
    // instancing".
    class RenderView
    {
    public:
        RenderView(std::string name, RenderResourceLedger& ledger);
        ~RenderView();

        RenderView(const RenderView&)            = delete;
        RenderView& operator=(const RenderView&) = delete;
        RenderView(RenderView&&)                 = delete;
        RenderView& operator=(RenderView&&)      = delete;

        const std::string& GetName() const { return m_Name; }

        RenderPipeline& GetPipeline() { return m_Pipeline; }
        RenderGraph&    GetGraph()    { return m_Pipeline.GetGraph(); }

        // Forwarded to the owned RenderGraph — see RenderGraph::SetStatsSuffix. Must be called (if
        // at all) before the first AddPass().
        void SetStatsSuffix(std::string suffix) { m_Pipeline.GetGraph().SetStatsSuffix(std::move(suffix)); }

        void AddPass(std::unique_ptr<IRenderPass> pass) { m_Pipeline.AddPass(std::move(pass)); }

        void Compile(RHI::IRHIDevice& device);

        // Desired size, set by the application (e.g. Renderer::SetViewSize) each frame; applied at
        // end-of-frame via Renderer's deferred-resize sweep, which compares this against the
        // currently-compiled size and calls Invalidate(ViewRelative) + MarkCompiled() when they
        // differ — see workflow/current-plan.md, "Descriptor / framebuffer lifetime on resize".
        void     SetDesiredSize(uint32_t width, uint32_t height) { m_DesiredWidth = width; m_DesiredHeight = height; }
        uint32_t GetDesiredWidth()   const { return m_DesiredWidth; }
        uint32_t GetDesiredHeight()  const { return m_DesiredHeight; }
        uint32_t GetCompiledWidth()  const { return m_CompiledWidth; }
        uint32_t GetCompiledHeight() const { return m_CompiledHeight; }
        void     MarkCompiled(uint32_t width, uint32_t height) { m_CompiledWidth = width; m_CompiledHeight = height; }

        void SetEnabled(bool enabled) { m_Enabled = enabled; }
        bool IsEnabled() const { return m_Enabled; }

        // camera == nullopt clears HasCamera rather than leaving the previous frame's camera in
        // place — called unconditionally every frame (see Renderer::SetViewCamera) so a view with
        // no camera this frame (e.g. the game view with no primary CameraComponent) reliably falls
        // back to an empty-bucket clear instead of showing a stale image.
        void SetCamera(const std::optional<HedgehogEngine::CameraData>& camera)
        {
            if (camera)
            {
                m_FrameData.Camera    = *camera;
                m_FrameData.HasCamera = true;
            }
            else
            {
                m_FrameData.HasCamera = false;
            }
        }
        void SetGizmo(const std::optional<HM::Matrix4x4>& gizmo) { m_FrameData.SelectedGizmo = gizmo; }
        ViewFrameData&       GetFrameData()       { return m_FrameData; }
        const ViewFrameData& GetFrameData() const { return m_FrameData; }

        // Scoped ledger name of this view's output colour texture, e.g. "scene/viewColor" — what
        // GuiPass imports via RenderGraphBuilder::ImportReadSampled.
        std::string OutputColorName() const;

        void Cleanup(RHI::IRHIDevice& device) { m_Pipeline.Cleanup(device); }

    private:
        std::string    m_Name;
        RenderPipeline m_Pipeline;

        ViewFrameData m_FrameData;

        uint32_t m_DesiredWidth   = 1;
        uint32_t m_DesiredHeight  = 1;
        uint32_t m_CompiledWidth  = 0;
        uint32_t m_CompiledHeight = 0;

        bool m_Enabled = true;
    };
}

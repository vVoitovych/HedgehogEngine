#include "UiPass.hpp"

#include "PassCommon.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
    namespace
    {
        // Clears target to opaque black: what the UI pass leaves when there is no UI to draw.
        void ClearColorTarget(RHI::IRHICommandList& cmd, RHI::IRHITexture& target)
        {
            RHI::RenderingAttachment color;
            color.Texture     = &target;
            color.LoadOp      = RHI::LoadOp::Clear;
            color.StoreOp     = RHI::StoreOp::Store;
            color.Clear.Color = { 0.0f, 0.0f, 0.0f, 1.0f };

            RHI::RenderingInfo info;
            info.ColorAttachments = { color };
            info.Width            = target.GetWidth();
            info.Height           = target.GetHeight();
            cmd.BeginRendering(info);
            cmd.EndRendering();
        }

        // The application's UI into the target (RENDERING.md section 7): the pass runs the frame
        // context's UiCallback and never draws anything itself. It samples the render targets the
        // view reads, so the compiler orders their writers first and leaves them readable.
        void BuildUi(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            graph.AddPass<TargetPassData>(invocation.GetName(),
                [&](RGPassBuilder& pass, TargetPassData& data)
                {
                    data.Context = graph.GetFrameContext();
                    data.Graph   = &graph;
                    if (data.Context && data.Context->Frame)
                    {
                        for (const RGTexture sampled : data.Context->Frame->UiSampledTargets)
                            pass.SampleTexture(sampled);
                    }
                    data.Target = pass.ColorTarget(invocation.GetSlot("target"));
                    invocation.SetSlot("target", data.Target);
                },
                [](TargetPassData& data, RHI::IRHICommandList& cmd)
                {
                    if (!data.Context)
                        return;
                    RHI::IRHITexture& target = *data.Graph->GetTexture(data.Target);
                    const UiCallback& ui     = data.Context->Frame->Ui;
                    if (ui)
                        ui(cmd, target);
                    else
                        ClearColorTarget(cmd, target);
                });
        }
    }

    PassTypeInfo GetUiPassType()
    {
        return { { "target" }, {}, &BuildUi };
    }
}

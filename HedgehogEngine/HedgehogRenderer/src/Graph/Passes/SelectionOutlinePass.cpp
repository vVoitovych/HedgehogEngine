#include "SelectionOutlinePass.hpp"

#include "PassCommon.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
    namespace
    {
        // The vertices of the fullscreen triangle Fullscreen/Triangle.vert makes from gl_VertexIndex.
        constexpr uint32_t FULLSCREEN_TRIANGLE_VERTICES = 3;

        struct SelectionOutlinePassData
        {
            RGTexture                Mask{};
            RGTexture                Color{};
            RenderGraphRuntime*      Graph   = nullptr;
            const GraphFrameContext* Context = nullptr;
        };

        void RecordSelectionOutline(SelectionOutlinePassData& data, RHI::IRHICommandList& cmd)
        {
            if (!data.Context || data.Context->Frame->OverlayInstances.empty())
                return;
            IGraphPassServices&           services = *data.Context->Services;
            const RHI::IRHIPipeline&      pipeline = services.GetPipeline(EnginePipeline::SelectionOutline);
            const RHI::IRHIDescriptorSet& mask     = services.AllocateSampledTexture(*data.Graph->GetTexture(data.Mask));
            RHI::IRHITexture&             color    = *data.Graph->GetTexture(data.Color);

            RHI::RenderingAttachment colorAttachment;
            colorAttachment.Texture = &color;
            colorAttachment.LoadOp  = RHI::LoadOp::Load; // the outline blends over the view
            colorAttachment.StoreOp = RHI::StoreOp::Store;

            RHI::RenderingInfo info;
            info.ColorAttachments = { colorAttachment };
            info.Width            = color.GetWidth();
            info.Height           = color.GetHeight();
            cmd.BeginRendering(info);

            cmd.BindPipeline(pipeline);
            cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(color.GetWidth()), static_cast<float>(color.GetHeight()),
                              0.0f, 1.0f });
            cmd.SetScissor({ 0, 0, color.GetWidth(), color.GetHeight() });
            cmd.BindDescriptorSet(pipeline, 0, mask);
            const SelectionOutlinePushConstants constants =
                MakeSelectionOutlinePushConstants(color.GetWidth(), color.GetHeight());
            cmd.PushConstants(pipeline, RHI::ShaderStage::Fragment, 0, sizeof(constants), &constants);
            cmd.Draw(FULLSCREEN_TRIANGLE_VERTICES, 1, 0, 0);
            cmd.EndRendering();
        }

        void BuildSelectionOutline(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            graph.AddPass<SelectionOutlinePassData>(invocation.GetName(),
                [&](RGPassBuilder& pass, SelectionOutlinePassData& data)
                {
                    data.Mask = invocation.GetSlot("mask");
                    pass.SampleTexture(data.Mask);
                    data.Color   = pass.ColorTarget(invocation.GetSlot("color"));
                    data.Graph   = &graph;
                    data.Context = graph.GetFrameContext();
                    invocation.SetSlot("color", data.Color);
                },
                [](SelectionOutlinePassData& data, RHI::IRHICommandList& cmd) { RecordSelectionOutline(data, cmd); });
        }
    }

    SelectionOutlinePushConstants MakeSelectionOutlinePushConstants(uint32_t width, uint32_t height)
    {
        SelectionOutlinePushConstants constants;
        if (width > 0 && height > 0)
        {
            constants.TexelSize[0] = 1.0f / static_cast<float>(width);
            constants.TexelSize[1] = 1.0f / static_cast<float>(height);
        }
        return constants;
    }

    PassTypeInfo GetSelectionOutlinePassType()
    {
        return { { "color", "mask" }, {}, &BuildSelectionOutline };
    }
}

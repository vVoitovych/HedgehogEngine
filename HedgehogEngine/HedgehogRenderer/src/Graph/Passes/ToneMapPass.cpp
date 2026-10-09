#include "ToneMapPass.hpp"

#include "PassCommon.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <cmath>

namespace Renderer
{
    namespace
    {
        // The vertices of the fullscreen triangle Fullscreen/Triangle.vert makes from gl_VertexIndex.
        constexpr uint32_t FULLSCREEN_TRIANGLE_VERTICES = 3;

        struct ToneMapPassData
        {
            RGTexture                Hdr{};
            RGTexture                Color{};
            RenderGraphRuntime*      Graph   = nullptr;
            const GraphFrameContext* Context = nullptr;
        };

        void RecordToneMap(ToneMapPassData& data, RHI::IRHICommandList& cmd)
        {
            if (!data.Context)
                return;
            IGraphPassServices&           services = *data.Context->Services;
            const RHI::IRHIPipeline&      pipeline = services.GetPipeline(EnginePipeline::ToneMap);
            const RHI::IRHIDescriptorSet& hdr      = services.AllocateSampledTexture(*data.Graph->GetTexture(data.Hdr));
            RHI::IRHITexture&             color    = *data.Graph->GetTexture(data.Color);

            RHI::RenderingAttachment colorAttachment;
            colorAttachment.Texture = &color;
            colorAttachment.LoadOp  = RHI::LoadOp::DontCare; // every pixel is written
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
            cmd.BindDescriptorSet(pipeline, 0, hdr);
            const ToneMapPushConstants constants = MakeToneMapPushConstants(data.Context->Frame->Exposure);
            cmd.PushConstants(pipeline, RHI::ShaderStage::Fragment, 0, sizeof(constants), &constants);
            cmd.Draw(FULLSCREEN_TRIANGLE_VERTICES, 1, 0, 0);
            cmd.EndRendering();
        }

        void BuildToneMap(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            graph.AddPass<ToneMapPassData>(invocation.GetName(),
                [&](RGPassBuilder& pass, ToneMapPassData& data)
                {
                    data.Hdr = invocation.GetSlot("hdr");
                    pass.SampleTexture(data.Hdr);
                    data.Color   = pass.ColorTarget(invocation.GetSlot("color"));
                    data.Graph   = &graph;
                    data.Context = graph.GetFrameContext();
                    invocation.SetSlot("color", data.Color);
                },
                [](ToneMapPassData& data, RHI::IRHICommandList& cmd) { RecordToneMap(data, cmd); });
        }
    }

    ToneMapPushConstants MakeToneMapPushConstants(float exposureEv)
    {
        ToneMapPushConstants constants;
        if (std::isfinite(exposureEv))
            constants.ExposureScale = std::exp2(exposureEv);
        return constants;
    }

    PassTypeInfo GetToneMapPassType()
    {
        return { { "hdr", "color" }, {}, &BuildToneMap };
    }
}

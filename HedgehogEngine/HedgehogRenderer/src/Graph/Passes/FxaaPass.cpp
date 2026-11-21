#include "FxaaPass.hpp"

#include "PassCommon.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
    namespace
    {
        constexpr uint32_t FULLSCREEN_TRIANGLE_VERTICES = 3;

        struct FxaaPassData
        {
            RGTexture                Ldr{};
            RGTexture                Color{};
            RenderGraphRuntime*      Graph   = nullptr;
            const GraphFrameContext* Context = nullptr;
        };

        void RecordFxaa(FxaaPassData& data, RHI::IRHICommandList& cmd)
        {
            if (!data.Context)
                return;
            IGraphPassServices&           services = *data.Context->Services;
            const RHI::IRHIPipeline&      pipeline = services.GetPipeline(EnginePipeline::Fxaa);
            RHI::IRHITexture&             ldr      = *data.Graph->GetTexture(data.Ldr);
            const RHI::IRHIDescriptorSet& source   = services.AllocateSampledTexture(ldr);
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
            cmd.BindDescriptorSet(pipeline, 0, source);
            const FxaaPushConstants constants =
                MakeFxaaPushConstants(ldr.GetWidth(), ldr.GetHeight(), data.Context->Frame->PostProcess.Fxaa);
            cmd.PushConstants(pipeline, RHI::ShaderStage::Fragment, 0, sizeof(constants), &constants);
            cmd.Draw(FULLSCREEN_TRIANGLE_VERTICES, 1, 0, 0);
            cmd.EndRendering();
        }

        void BuildFxaa(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            graph.AddPass<FxaaPassData>(invocation.GetName(),
                [&](RGPassBuilder& pass, FxaaPassData& data)
                {
                    data.Ldr = invocation.GetSlot("ldr");
                    pass.SampleTexture(data.Ldr);
                    data.Color   = pass.ColorTarget(invocation.GetSlot("color"));
                    data.Graph   = &graph;
                    data.Context = graph.GetFrameContext();
                    invocation.SetSlot("color", data.Color);
                },
                [](FxaaPassData& data, RHI::IRHICommandList& cmd) { RecordFxaa(data, cmd); });
        }
    }

    FxaaPushConstants MakeFxaaPushConstants(uint32_t width, uint32_t height, bool enabled)
    {
        FxaaPushConstants constants;
        constants.TexelWidth  = width > 0 ? 1.0f / static_cast<float>(width) : 0.0f;
        constants.TexelHeight = height > 0 ? 1.0f / static_cast<float>(height) : 0.0f;
        constants.Enabled     = enabled ? 1u : 0u;
        return constants;
    }

    PassTypeInfo GetFxaaPassType()
    {
        return { { "ldr", "color" }, {}, &BuildFxaa };
    }
}

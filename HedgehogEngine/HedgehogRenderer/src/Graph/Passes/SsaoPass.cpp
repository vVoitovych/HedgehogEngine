#include "SsaoPass.hpp"

#include "PassCommon.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <algorithm>
#include <cmath>

namespace Renderer
{
    namespace
    {
        constexpr uint32_t FULLSCREEN_TRIANGLE_VERTICES = 3;

        struct SsaoPassData
        {
            RGTexture                Depth{};
            RGTexture                Raw{};
            RenderGraphRuntime*      Graph   = nullptr;
            const GraphFrameContext* Context = nullptr;
        };

        struct SsaoBlurPassData
        {
            RGTexture                Depth{};
            RGTexture                Raw{};
            RGTexture                Ao{};
            RenderGraphRuntime*      Graph   = nullptr;
            const GraphFrameContext* Context = nullptr;
        };

        float OrDefault(float value, float fallback)
        {
            return std::isfinite(value) ? std::max(0.0f, value) : fallback;
        }

        void BeginTarget(RHI::IRHICommandList& cmd, RHI::IRHITexture& target, RHI::LoadOp load)
        {
            RHI::RenderingAttachment attachment;
            attachment.Texture    = &target;
            attachment.LoadOp     = load;
            attachment.StoreOp    = RHI::StoreOp::Store;
            attachment.Clear.Color = { 1.0f, 1.0f, 1.0f, 1.0f };

            RHI::RenderingInfo info;
            info.ColorAttachments = { attachment };
            info.Width            = target.GetWidth();
            info.Height           = target.GetHeight();
            cmd.BeginRendering(info);
            cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(target.GetWidth()), static_cast<float>(target.GetHeight()),
                              0.0f, 1.0f });
            cmd.SetScissor({ 0, 0, target.GetWidth(), target.GetHeight() });
        }

        void RecordSsao(SsaoPassData& data, RHI::IRHICommandList& cmd)
        {
            if (!data.Context || !data.Context->Frame->PostProcess.Ssao)
                return;
            IGraphPassServices&           services = *data.Context->Services;
            const RHI::IRHIPipeline&      pipeline = services.GetPipeline(EnginePipeline::Ssao);
            RHI::IRHITexture&             depth    = *data.Graph->GetTexture(data.Depth);
            const RHI::IRHIDescriptorSet& depthSet = services.AllocateSampledTexture(depth);
            RHI::IRHITexture&             raw      = *data.Graph->GetTexture(data.Raw);

            BeginTarget(cmd, raw, RHI::LoadOp::DontCare);
            cmd.BindPipeline(pipeline);
            cmd.BindDescriptorSet(pipeline, 0, depthSet);
            const SsaoPushConstants constants = MakeSsaoPushConstants(data.Context->Frame->Proj, data.Context->Frame->PostProcess,
                                                                      depth.GetWidth(), depth.GetHeight());
            cmd.PushConstants(pipeline, RHI::ShaderStage::Fragment, 0, sizeof(constants), &constants);
            cmd.Draw(FULLSCREEN_TRIANGLE_VERTICES, 1, 0, 0);
            cmd.EndRendering();
        }

        void RecordBlur(SsaoBlurPassData& data, RHI::IRHICommandList& cmd)
        {
            if (!data.Context)
                return;
            RHI::IRHITexture& ao = *data.Graph->GetTexture(data.Ao);
            if (!data.Context->Frame->PostProcess.Ssao)
            {
                // No occlusion: white, which Forward multiplies its ambient light by.
                BeginTarget(cmd, ao, RHI::LoadOp::Clear);
                cmd.EndRendering();
                return;
            }
            IGraphPassServices&           services = *data.Context->Services;
            const RHI::IRHIPipeline&      pipeline = services.GetPipeline(EnginePipeline::SsaoBlur);
            RHI::IRHITexture&             raw      = *data.Graph->GetTexture(data.Raw);
            const RHI::IRHIDescriptorSet& rawSet   = services.AllocateSampledTexture(raw);
            const RHI::IRHIDescriptorSet& depthSet = services.AllocateSampledTexture(*data.Graph->GetTexture(data.Depth));

            BeginTarget(cmd, ao, RHI::LoadOp::DontCare);
            cmd.BindPipeline(pipeline);
            cmd.BindDescriptorSet(pipeline, 0, rawSet);
            cmd.BindDescriptorSet(pipeline, 1, depthSet);
            const SsaoBlurPushConstants constants =
                MakeSsaoBlurPushConstants(data.Context->Frame->Proj, raw.GetWidth(), raw.GetHeight());
            cmd.PushConstants(pipeline, RHI::ShaderStage::Fragment, 0, sizeof(constants), &constants);
            cmd.Draw(FULLSCREEN_TRIANGLE_VERTICES, 1, 0, 0);
            cmd.EndRendering();
        }

        void BuildSsao(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            // The raw occlusion, the size ao is (half the view in the shipped graphs).
            const RGTexture raw = graph.CreateTexture({ invocation.GetName() + " raw", RHI::Format::R8Unorm,
                                                        RGSizePolicy::MakeRelativeToResult(0.5f),
                                                        RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled });
            RGTexture rawWritten;
            graph.AddPass<SsaoPassData>(invocation.GetName(),
                [&](RGPassBuilder& pass, SsaoPassData& data)
                {
                    data.Depth = invocation.GetSlot("depth");
                    pass.SampleTexture(data.Depth);
                    rawWritten = data.Raw = pass.ColorTarget(raw);
                    data.Graph   = &graph;
                    data.Context = graph.GetFrameContext();
                },
                [](SsaoPassData& data, RHI::IRHICommandList& cmd) { RecordSsao(data, cmd); });
            graph.AddPass<SsaoBlurPassData>(invocation.GetName() + " blur",
                [&](RGPassBuilder& pass, SsaoBlurPassData& data)
                {
                    data.Depth = invocation.GetSlot("depth");
                    pass.SampleTexture(data.Depth);
                    data.Raw = rawWritten;
                    pass.SampleTexture(data.Raw);
                    data.Ao      = pass.ColorTarget(invocation.GetSlot("ao"));
                    data.Graph   = &graph;
                    data.Context = graph.GetFrameContext();
                    invocation.SetSlot("ao", data.Ao);
                },
                [](SsaoBlurPassData& data, RHI::IRHICommandList& cmd) { RecordBlur(data, cmd); });
        }
    }

    SsaoProjection MakeSsaoProjection(const HM::Matrix4x4& proj)
    {
        SsaoProjection p;
        p.A = proj[0][0];
        p.B = proj[1][1];
        p.C = proj[2][2];
        p.D = proj[3][2];
        p.E = proj[2][3];
        p.F = proj[3][3];
        p.G = proj[3][0];
        p.H = proj[3][1];
        return p;
    }

    SsaoPushConstants MakeSsaoPushConstants(const HM::Matrix4x4& proj, const PostProcessSettings& settings,
                                            uint32_t depthWidth, uint32_t depthHeight)
    {
        const PostProcessSettings defaults;
        SsaoPushConstants         constants;
        constants.Projection  = MakeSsaoProjection(proj);
        constants.Radius      = OrDefault(settings.SsaoRadius, defaults.SsaoRadius);
        constants.Intensity   = OrDefault(settings.SsaoIntensity, defaults.SsaoIntensity);
        constants.Bias        = OrDefault(settings.SsaoBias, defaults.SsaoBias);
        constants.TexelWidth  = depthWidth > 0 ? 1.0f / static_cast<float>(depthWidth) : 0.0f;
        constants.TexelHeight = depthHeight > 0 ? 1.0f / static_cast<float>(depthHeight) : 0.0f;
        return constants;
    }

    SsaoBlurPushConstants MakeSsaoBlurPushConstants(const HM::Matrix4x4& proj, uint32_t rawWidth, uint32_t rawHeight)
    {
        const SsaoProjection  p = MakeSsaoProjection(proj);
        SsaoBlurPushConstants constants;
        constants.C           = p.C;
        constants.D           = p.D;
        constants.E           = p.E;
        constants.F           = p.F;
        constants.TexelWidth  = rawWidth > 0 ? 1.0f / static_cast<float>(rawWidth) : 0.0f;
        constants.TexelHeight = rawHeight > 0 ? 1.0f / static_cast<float>(rawHeight) : 0.0f;
        return constants;
    }

    PassTypeInfo GetSsaoPassType()
    {
        return { { "depth", "ao" }, {}, &BuildSsao };
    }
}

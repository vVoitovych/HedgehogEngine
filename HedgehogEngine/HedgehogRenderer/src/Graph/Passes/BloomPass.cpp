#include "BloomPass.hpp"

#include "PassCommon.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <array>
#include <cmath>
#include <string>

namespace Renderer
{
    namespace
    {
        constexpr uint32_t    FULLSCREEN_TRIANGLE_VERTICES = 3;
        constexpr RHI::Format BLOOM_FORMAT                 = RHI::Format::R16G16B16A16Float;

        enum class BloomStep
        {
            Down,  // Source into Target, written whole
            Up,    // Source added onto Target
            Final, // Source into bloom, cleared first; with bloom off only the clear
        };

        struct BloomPassData
        {
            RGTexture                Source{};
            RGTexture                Target{};
            BloomStep                Step    = BloomStep::Down;
            bool                     First   = false; // the first downsample, from the HDR target
            RenderGraphRuntime*      Graph   = nullptr;
            const GraphFrameContext* Context = nullptr;
        };

        float TexelOf(uint32_t size)
        {
            return size > 0 ? 1.0f / static_cast<float>(size) : 0.0f;
        }

        void RecordBloom(BloomPassData& data, RHI::IRHICommandList& cmd)
        {
            if (!data.Context)
                return;
            const bool        enabled = data.Context->Frame->PostProcess.Bloom;
            RHI::IRHITexture& target  = *data.Graph->GetTexture(data.Target);
            if (!enabled && data.Step != BloomStep::Final)
                return;

            RHI::RenderingAttachment attachment;
            attachment.Texture     = &target;
            attachment.LoadOp      = data.Step == BloomStep::Up ? RHI::LoadOp::Load
                                     : data.Step == BloomStep::Final ? RHI::LoadOp::Clear
                                                                     : RHI::LoadOp::DontCare;
            attachment.StoreOp     = RHI::StoreOp::Store;
            attachment.Clear.Color = { 0.0f, 0.0f, 0.0f, 0.0f };
            RHI::RenderingInfo info;
            info.ColorAttachments = { attachment };
            info.Width            = target.GetWidth();
            info.Height           = target.GetHeight();
            cmd.BeginRendering(info);
            if (!enabled)
            {
                cmd.EndRendering(); // bloom off: black, which ToneMap adds as nothing
                return;
            }

            IGraphPassServices&           services = *data.Context->Services;
            const bool                    down     = data.Step == BloomStep::Down;
            const RHI::IRHIPipeline&      pipeline = services.GetPipeline(down ? EnginePipeline::BloomDownsample
                                                                               : EnginePipeline::BloomUpsample);
            RHI::IRHITexture&             source   = *data.Graph->GetTexture(data.Source);
            const RHI::IRHIDescriptorSet& set      = services.AllocateSampledTexture(source);
            cmd.BindPipeline(pipeline);
            cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(target.GetWidth()), static_cast<float>(target.GetHeight()),
                              0.0f, 1.0f });
            cmd.SetScissor({ 0, 0, target.GetWidth(), target.GetHeight() });
            cmd.BindDescriptorSet(pipeline, 0, set);
            if (down)
            {
                const BloomDownsamplePushConstants constants = MakeBloomDownsamplePushConstants(
                    source.GetWidth(), source.GetHeight(), data.Context->Frame->PostProcess, data.First);
                cmd.PushConstants(pipeline, RHI::ShaderStage::Fragment, 0, sizeof(constants), &constants);
            }
            else
            {
                const BloomUpsamplePushConstants constants = MakeBloomUpsamplePushConstants(source.GetWidth(), source.GetHeight());
                cmd.PushConstants(pipeline, RHI::ShaderStage::Fragment, 0, sizeof(constants), &constants);
            }
            cmd.Draw(FULLSCREEN_TRIANGLE_VERTICES, 1, 0, 0);
            cmd.EndRendering();
        }

        void AddStep(RenderGraphRuntime& graph, const std::string& name, RGTexture source, RGTexture& target, BloomStep step,
                     bool first)
        {
            graph.AddPass<BloomPassData>(name,
                [&](RGPassBuilder& pass, BloomPassData& data)
                {
                    data.Source = source;
                    pass.SampleTexture(source);
                    target       = data.Target = pass.ColorTarget(target);
                    data.Step    = step;
                    data.First   = first;
                    data.Graph   = &graph;
                    data.Context = graph.GetFrameContext();
                },
                [](BloomPassData& data, RHI::IRHICommandList& cmd) { RecordBloom(data, cmd); });
        }

        void BuildBloom(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            const std::string& name = invocation.GetName();
            std::array<RGTexture, BLOOM_LEVELS> levels{};
            float scale = 0.5f;
            for (uint32_t i = 0; i < BLOOM_LEVELS; ++i, scale *= 0.5f)
                levels[i] = graph.CreateTexture({ name + " level " + std::to_string(i), BLOOM_FORMAT,
                                                  RGSizePolicy::MakeRelativeToResult(scale),
                                                  RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled });

            RGTexture source = invocation.GetSlot("hdr");
            for (uint32_t i = 0; i < BLOOM_LEVELS; ++i)
            {
                AddStep(graph, name + " down " + std::to_string(i), source, levels[i], BloomStep::Down, i == 0);
                source = levels[i];
            }
            for (uint32_t i = BLOOM_LEVELS - 1; i > 0; --i)
                AddStep(graph, name + " up " + std::to_string(i - 1), levels[i], levels[i - 1], BloomStep::Up, false);

            RGTexture bloom = invocation.GetSlot("bloom");
            AddStep(graph, name, levels[0], bloom, BloomStep::Final, false);
            invocation.SetSlot("bloom", bloom);
        }
    }

    BloomDownsamplePushConstants MakeBloomDownsamplePushConstants(uint32_t sourceWidth, uint32_t sourceHeight,
                                                                  const PostProcessSettings& settings, bool prefilter)
    {
        const PostProcessSettings    defaults;
        const auto                   valid = [](float value) { return std::isfinite(value) && value >= 0.0f; };
        BloomDownsamplePushConstants constants;
        constants.TexelWidth  = TexelOf(sourceWidth);
        constants.TexelHeight = TexelOf(sourceHeight);
        constants.Threshold   = valid(settings.BloomThreshold) ? settings.BloomThreshold : defaults.BloomThreshold;
        constants.Knee        = constants.Threshold * (valid(settings.BloomKnee) ? settings.BloomKnee : defaults.BloomKnee);
        constants.Prefilter   = prefilter ? 1u : 0u;
        return constants;
    }

    BloomUpsamplePushConstants MakeBloomUpsamplePushConstants(uint32_t sourceWidth, uint32_t sourceHeight)
    {
        BloomUpsamplePushConstants constants;
        constants.TexelWidth  = TexelOf(sourceWidth);
        constants.TexelHeight = TexelOf(sourceHeight);
        return constants;
    }

    PassTypeInfo GetBloomPassType()
    {
        return { { "hdr", "bloom" }, {}, &BuildBloom };
    }
}

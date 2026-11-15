#include "ParticlesPass.hpp"

#include "PassCommon.hpp"

#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <algorithm>

namespace Renderer
{
    namespace
    {
        constexpr uint32_t ADDITIVE_BLEND     = 1; // HedgehogEngine's ParticleBlend::Additive
        constexpr uint32_t DRAW_ARGS_STRIDE   = 16;

        void RecordParticles(ForwardPassData& data, RHI::IRHICommandList& cmd)
        {
            if (!data.Context)
                return;
            const GraphFrameData& frame = *data.Context->Frame;
            if (!frame.Particles || frame.Particles->Emitters.empty())
                return;
            const ParticleFrameData&      particles = *frame.Particles;
            IGraphPassServices&           services  = *data.Context->Services;
            const RHI::IRHIDescriptorSet* set       = services.GetParticleSet();
            if (!set || !frame.UiSolidTexture)
                return;
            RHI::IRHITexture& color = *data.Graph->GetTexture(data.Color);
            RHI::IRHITexture& depth = *data.Graph->GetTexture(data.Depth);
            const GpuScope    scope(cmd, "Particles");

            RHI::RenderingAttachment colorAttachment;
            colorAttachment.Texture = &color;
            colorAttachment.LoadOp  = RHI::LoadOp::Load;
            colorAttachment.StoreOp = RHI::StoreOp::Store;
            RHI::RenderingAttachment depthAttachment;
            depthAttachment.Texture = &depth;
            depthAttachment.LoadOp  = RHI::LoadOp::Load;
            depthAttachment.StoreOp = RHI::StoreOp::Store;
            RHI::RenderingInfo info;
            info.ColorAttachments = { colorAttachment };
            info.DepthAttachment  = depthAttachment;
            info.Width            = color.GetWidth();
            info.Height           = color.GetHeight();
            cmd.BeginRendering(info);
            cmd.SetViewport({ 0.0f, 0.0f, static_cast<float>(color.GetWidth()), static_cast<float>(color.GetHeight()), 0.0f, 1.0f });
            cmd.SetScissor({ 0, 0, color.GetWidth(), color.GetHeight() });

            const RHI::IRHIPipeline* bound = nullptr;
            for (uint32_t i = 0; i < particles.Emitters.size(); ++i)
            {
                const ParticleEmitterUniform& emitter  = particles.Emitters[i];
                const RHI::IRHIPipeline&      pipeline = services.GetPipeline(
                    emitter.Blend == ADDITIVE_BLEND ? EnginePipeline::ParticlesAdditive : EnginePipeline::ParticlesAlpha);
                if (&pipeline != bound)
                {
                    cmd.BindPipeline(pipeline);
                    cmd.BindDescriptorSet(pipeline, 0, *set);
                    bound = &pipeline;
                }
                const RHI::IRHIDescriptorSet* texture = i < particles.Textures.size() ? particles.Textures[i] : nullptr;
                cmd.BindDescriptorSet(pipeline, 1, texture ? *texture : *frame.UiSolidTexture);
                const ParticlesPushConstants constants = MakeParticlesPushConstants(frame.View, frame.Proj, i, texture != nullptr);
                cmd.PushConstants(pipeline, RHI::ShaderStage::Vertex | RHI::ShaderStage::Fragment, 0, sizeof(constants), &constants);
                cmd.DrawIndirect(*particles.Get(ParticleBuffer::Slots), ParticleDrawArgsOffset(emitter.SlotRow), 1, DRAW_ARGS_STRIDE);
            }
            cmd.EndRendering();
        }

        void BuildParticles(RenderGraphRuntime& graph, PassInvocation& invocation)
        {
            graph.AddPass<ForwardPassData>(invocation.GetName(),
                [&](RGPassBuilder& pass, ForwardPassData& data)
                {
                    data.Depth = invocation.GetSlot("depth");
                    pass.DepthReadOnly(data.Depth);
                    data.Graph   = &graph;
                    data.Context = graph.GetFrameContext();
                    // The simulation's output: read here, so it runs first and its writes are seen.
                    if (data.Context && data.Context->Frame->Particles)
                    {
                        const ParticleGraphBuffers& buffers = data.Context->Frame->ParticleBuffers;
                        pass.ReadBuffer(buffers.State);
                        pass.ReadBuffer(buffers.AliveOut);
                        pass.ReadIndirectArgs(buffers.Slots);
                    }
                    data.Color = pass.ColorTarget(invocation.GetSlot("color"));
                    invocation.SetSlot("color", data.Color);
                },
                [](ForwardPassData& data, RHI::IRHICommandList& cmd) { RecordParticles(data, cmd); });
        }
    }

    ParticlesPushConstants MakeParticlesPushConstants(const HM::Matrix4x4& view, const HM::Matrix4x4& proj,
                                                      uint32_t emitterIndex, bool hasTexture)
    {
        ParticlesPushConstants constants;
        const HM::Matrix4x4 viewProj = proj * view;
        std::copy_n(viewProj.GetBuffer(), 16, constants.ViewProj);
        // A view matrix's rows are the camera's axes in the world (Matrix4x4 stores columns).
        const float* m = view.GetBuffer();
        constants.CameraRight[0] = m[0];
        constants.CameraRight[1] = m[4];
        constants.CameraRight[2] = m[8];
        constants.CameraUp[0]    = m[1];
        constants.CameraUp[1]    = m[5];
        constants.CameraUp[2]    = m[9];
        constants.EmitterIndex   = emitterIndex;
        constants.HasTexture     = hasTexture ? 1u : 0u;
        return constants;
    }

    PassTypeInfo GetParticlesPassType()
    {
        return { { "color", "depth" }, {}, &BuildParticles };
    }
}

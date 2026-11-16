#include "ParticlesPass.hpp"

#include "ParticleSimulatePass.hpp"
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

        struct ParticleSortData
        {
            const GraphFrameContext* Context = nullptr;
        };

        struct ParticleDepthCopyData
        {
            const GraphFrameContext* Context = nullptr;
            RenderGraphRuntime*      Graph   = nullptr;
            RGTexture                Depth;
        };

        bool HasDepthCollision(const ParticleFrameData& particles)
        {
            for (const ParticleEmitterUniform& emitter : particles.Emitters)
                if (emitter.Collision == PARTICLE_COLLISION_DEPTH_BUFFER)
                    return true;
            return false;
        }

        // The view's depth into the collision depth, one thread per texel.
        void RecordParticleDepthCopy(const ParticleDepthCopyData& data, RHI::IRHICommandList& cmd)
        {
            IGraphPassServices&           services = *data.Context->Services;
            const RHI::IRHIDescriptorSet* set      = services.GetParticleSet();
            if (!set)
                return;
            const GpuScope           scope(cmd, "Collision depth");
            const RHI::IRHIPipeline& copy = services.GetComputePipeline(EngineComputePipeline::ParticleCopyDepth);
            cmd.BindPipeline(copy);
            cmd.BindDescriptorSet(copy, 0, *set);
            cmd.BindDescriptorSet(copy, 1, services.AllocateSampledTexture(*data.Graph->GetTexture(data.Depth)));
            cmd.Dispatch(PARTICLE_COLLISION_DEPTH_SIZE / 8, PARTICLE_COLLISION_DEPTH_SIZE / 8, 1);
        }

        bool HasAlphaEmitters(const ParticleFrameData& particles)
        {
            for (const ParticleEmitterUniform& emitter : particles.Emitters)
                if (emitter.Blend != ADDITIVE_BLEND)
                    return true;
            return false;
        }

        // Each alpha emitter's sort list for this view: keys, then every bitonic step, each reading
        // what the one before wrote.
        void RecordParticleSort(const ParticleSortData& data, RHI::IRHICommandList& cmd)
        {
            const GraphFrameData&         frame     = *data.Context->Frame;
            const ParticleFrameData&      particles = *frame.Particles;
            IGraphPassServices&           services  = *data.Context->Services;
            const RHI::IRHIDescriptorSet* set       = services.GetParticleSet();
            if (!set)
                return;
            const GpuScope           scope(cmd, "Sort");
            const RHI::BufferBarrier barrier[] = { { particles.Get(ParticleBuffer::Sort), RHI::ResourceState::UnorderedAccess,
                                                     RHI::ResourceState::UnorderedAccess } };
            const RHI::IRHIPipeline& keys = services.GetComputePipeline(EngineComputePipeline::ParticleSortKeys);
            const RHI::IRHIPipeline& step = services.GetComputePipeline(EngineComputePipeline::ParticleSortStep);
            for (uint32_t i = 0; i < particles.Emitters.size(); ++i)
            {
                if (particles.Emitters[i].Blend == ADDITIVE_BLEND)
                    continue;
                const uint32_t            length = ParticleSortLength(particles.Emitters[i].SlotCount);
                ParticleSortPushConstants push   = MakeParticleSortPushConstants(frame.View, i, length);
                cmd.BindPipeline(keys);
                cmd.BindDescriptorSet(keys, 0, *set);
                cmd.PushConstants(keys, RHI::ShaderStage::Compute, 0, sizeof(push), &push);
                cmd.Dispatch(ParticleGroups(length), 1, 1);
                cmd.Barrier({}, barrier);
                if (length < 2)
                    continue;
                cmd.BindPipeline(step);
                cmd.BindDescriptorSet(step, 0, *set);
                ForEachBitonicStep(length, [&](uint32_t k, uint32_t j)
                {
                    push.StepK = k;
                    push.StepJ = j;
                    cmd.PushConstants(step, RHI::ShaderStage::Compute, 0, sizeof(push), &push);
                    cmd.Dispatch(ParticleGroups(length), 1, 1);
                    cmd.Barrier({}, barrier);
                });
            }
        }

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
            // The sort list this view's alpha emitters draw through, written after the last view's
            // draw read it (the latest version), so the views take turns with one list.
            const GraphFrameContext* context = graph.GetFrameContext();
            RGBuffer                 sorted;

            // The shadow view's depth, for the next frame's DepthBuffer collision.
            if (context && context->Frame->Particles && context->Frame->ParticleCollision
                && HasDepthCollision(*context->Frame->Particles))
            {
                const ParticleGraphBuffers& buffers = context->Frame->ParticleBuffers;
                graph.AddPass<ParticleDepthCopyData>(invocation.GetName() + " collision depth",
                    [&](RGPassBuilder& pass, ParticleDepthCopyData& data)
                    {
                        data.Depth = invocation.GetSlot("depth");
                        pass.SampleTexture(data.Depth);
                        const RGResourceRecord* record = graph.GetDescription().FindResource(buffers.CollisionDepth.Id);
                        pass.WriteBuffer(RGBuffer{ buffers.CollisionDepth.Id, record->LatestVersion });
                        pass.SetSideEffect();
                        data.Context = context;
                        data.Graph   = &graph;
                        ParticleCollisionDepth& collision = *context->Frame->ParticleCollision;
                        collision.ViewProj = context->Frame->Proj * context->Frame->View;
                        collision.Written  = true;
                    },
                    [](ParticleDepthCopyData& data, RHI::IRHICommandList& cmd) { RecordParticleDepthCopy(data, cmd); });
            }
            if (context && context->Frame->Particles && HasAlphaEmitters(*context->Frame->Particles))
            {
                const ParticleGraphBuffers& buffers = context->Frame->ParticleBuffers;
                graph.AddPass<ParticleSortData>(invocation.GetName() + " sort",
                    [&](RGPassBuilder& pass, ParticleSortData& data)
                    {
                        pass.ReadBuffer(buffers.State);
                        pass.ReadBuffer(buffers.AliveOut);
                        pass.ReadBuffer(buffers.Slots);
                        const RGResourceRecord* record = graph.GetDescription().FindResource(buffers.Sort.Id);
                        sorted       = pass.WriteBuffer(RGBuffer{ buffers.Sort.Id, record->LatestVersion });
                        data.Context = context;
                    },
                    [](ParticleSortData& data, RHI::IRHICommandList& cmd) { RecordParticleSort(data, cmd); });
            }

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
                        if (sorted.Id == buffers.Sort.Id)
                            pass.ReadBuffer(sorted);
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

    ParticleSortPushConstants MakeParticleSortPushConstants(const HM::Matrix4x4& view, uint32_t emitterIndex, uint32_t count)
    {
        ParticleSortPushConstants push;
        push.EmitterIndex = emitterIndex;
        push.Count        = count;
        // The eye is -(R^T t): the view's rows are the camera's axes, its last column the translation.
        const float* m = view.GetBuffer();
        for (int axis = 0; axis < 3; ++axis)
            push.Eye[axis] = -(m[axis * 4] * m[12] + m[axis * 4 + 1] * m[13] + m[axis * 4 + 2] * m[14]);
        return push;
    }

    PassTypeInfo GetParticlesPassType()
    {
        return { { "color", "depth" }, {}, &BuildParticles };
    }
}

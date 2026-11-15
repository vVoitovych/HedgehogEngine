#include "ParticleSimulatePass.hpp"
#include "PassCommon.hpp"

#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/IRHICommandList.hpp"

#include <array>

namespace Renderer
{
    namespace
    {
        struct ParticleSimulateData
        {
            const ParticleFrameData* Particles = nullptr;
            IGraphPassServices*      Services  = nullptr;
        };

        void RecordParticleSimulation(const ParticleSimulateData& data, RHI::IRHICommandList& cmd)
        {
            const ParticleFrameData&      particles = *data.Particles;
            IGraphPassServices&           services  = *data.Services;
            const RHI::IRHIDescriptorSet& set       = services.AllocateParticleSet(particles);
            const uint32_t                count     = static_cast<uint32_t>(particles.Emitters.size());

            const auto bind = [&](EngineComputePipeline which) -> const RHI::IRHIPipeline&
            {
                const RHI::IRHIPipeline& pipeline = services.GetComputePipeline(which);
                cmd.BindPipeline(pipeline);
                cmd.BindDescriptorSet(pipeline, 0, set);
                return pipeline;
            };
            const auto dispatch = [&](const RHI::IRHIPipeline& pipeline, uint32_t emitter, uint32_t threads)
            {
                const ParticlePushConstants push{ emitter, count };
                cmd.PushConstants(pipeline, RHI::ShaderStage::Compute, 0, sizeof(push), &push);
                cmd.Dispatch(ParticleGroups(threads), 1, 1);
            };
            // Every stage reads what the one before wrote, in any of the buffers.
            const auto stageBarrier = [&]
            {
                const std::array<RHI::BufferBarrier, 5> barriers = { {
                    { particles.State, RHI::ResourceState::UnorderedAccess, RHI::ResourceState::UnorderedAccess },
                    { particles.DeadList, RHI::ResourceState::UnorderedAccess, RHI::ResourceState::UnorderedAccess },
                    { particles.AliveIn, RHI::ResourceState::UnorderedAccess, RHI::ResourceState::UnorderedAccess },
                    { particles.AliveOut, RHI::ResourceState::UnorderedAccess, RHI::ResourceState::UnorderedAccess },
                    { particles.Slots, RHI::ResourceState::UnorderedAccess, RHI::ResourceState::UnorderedAccess },
                } };
                cmd.Barrier({}, barriers);
            };

            bool reset = false;
            for (const uint8_t flag : particles.Reset)
                reset = reset || flag != 0;
            if (reset)
            {
                const RHI::IRHIPipeline& pipeline = bind(EngineComputePipeline::ParticleReset);
                for (uint32_t i = 0; i < count; ++i)
                    if (particles.Reset[i] != 0)
                        dispatch(pipeline, i, particles.Emitters[i].SlotCount);
                stageBarrier();
            }

            bool spawns = false;
            for (const ParticleEmitterUniform& emitter : particles.Emitters)
                spawns = spawns || emitter.SpawnCount > 0;
            if (spawns)
            {
                const RHI::IRHIPipeline& pipeline = bind(EngineComputePipeline::ParticleEmit);
                for (uint32_t i = 0; i < count; ++i)
                    if (particles.Emitters[i].SpawnCount > 0)
                        dispatch(pipeline, i, particles.Emitters[i].SpawnCount);
                stageBarrier();
            }

            // Every slot moves on, paused ones too (by 0 s), so each frame's survivors are in the
            // list the next frame reads.
            const RHI::IRHIPipeline& simulate = bind(EngineComputePipeline::ParticleSimulate);
            for (uint32_t i = 0; i < count; ++i)
                dispatch(simulate, i, particles.Emitters[i].SlotCount);
            stageBarrier();

            dispatch(bind(EngineComputePipeline::ParticleFinish), 0, count);
        }
    }

    void DeclareParticleSimulation(RenderGraphRuntime& graph, IGraphPassServices& services, const ParticleFrameData& particles)
    {
        struct Imported
        {
            const char*      Name;
            RHI::IRHIBuffer* Buffer;
        };
        const std::array<Imported, 5> buffers = { { { "ParticleState", particles.State },
                                                    { "ParticleDeadList", particles.DeadList },
                                                    { "ParticleAliveIn", particles.AliveIn },
                                                    { "ParticleAliveOut", particles.AliveOut },
                                                    { "ParticleSlots", particles.Slots } } };
        std::array<RGBuffer, 5> handles{};
        for (size_t i = 0; i < buffers.size(); ++i)
        {
            handles[i] = graph.ImportBuffer(buffers[i].Name, buffers[i].Buffer->GetSize(), false, particles.InitialState);
            graph.BindImportedBuffer(handles[i], buffers[i].Buffer);
        }

        graph.AddPass<ParticleSimulateData>(
            "ParticleSimulate",
            [&](RGPassBuilder& pass, ParticleSimulateData& data)
            {
                for (const RGBuffer buffer : handles)
                    pass.WriteBuffer(buffer);
                pass.SetSideEffect();
                data.Particles = &particles;
                data.Services  = &services;
            },
            [](ParticleSimulateData& data, RHI::IRHICommandList& cmd)
            {
                const GpuScope scope(cmd, "Particles");
                RecordParticleSimulation(data, cmd);
            });
    }
}

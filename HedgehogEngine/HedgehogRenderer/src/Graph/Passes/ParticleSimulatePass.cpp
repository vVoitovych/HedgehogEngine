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
            const auto dispatch = [&](const RHI::IRHIPipeline& pipeline, uint32_t emitter, uint32_t threads,
                                      uint32_t child = 0, uint32_t childCount = 0)
            {
                const ParticlePushConstants push{ emitter, count, child, childCount };
                cmd.PushConstants(pipeline, RHI::ShaderStage::Compute, 0, sizeof(push), &push);
                cmd.Dispatch(ParticleGroups(threads), 1, 1);
            };
            // Every stage reads what the one before wrote, in any of the buffers.
            const auto stageBarrier = [&]
            {
                std::array<RHI::BufferBarrier, PARTICLE_BUFFER_COUNT> barriers{};
                for (size_t i = 0; i < PARTICLE_BUFFER_COUNT; ++i)
                    barriers[i] = { particles.Buffers[i], RHI::ResourceState::UnorderedAccess, RHI::ResourceState::UnorderedAccess };
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

            // Every death of a parent may spawn its children; a parent's own parent spawns into it
            // only after its deaths are read, at the next depth.
            if (!particles.SubEmitters.empty())
            {
                const RHI::IRHIPipeline& spawn = bind(EngineComputePipeline::ParticleSpawnFromDeaths);
                for (size_t i = 0; i < particles.SubEmitters.size(); ++i)
                {
                    const ParticleSubEmitterDispatch& sub = particles.SubEmitters[i];
                    if (i > 0 && particles.SubEmitters[i - 1].Depth != sub.Depth)
                        stageBarrier();
                    dispatch(spawn, sub.Parent, particles.Emitters[sub.Parent].SlotCount * sub.Count, sub.Child, sub.Count);
                }
                stageBarrier();
            }

            dispatch(bind(EngineComputePipeline::ParticleFinish), 0, count);
        }
    }

    ParticleGraphBuffers DeclareParticleSimulation(RenderGraphRuntime& graph, IGraphPassServices& services,
                                                   const ParticleFrameData& particles)
    {
        std::array<RGBuffer, PARTICLE_BUFFER_COUNT> handles{};
        for (size_t i = 0; i < PARTICLE_BUFFER_COUNT; ++i)
        {
            handles[i] = graph.ImportBuffer(PARTICLE_BUFFER_NAMES[i], particles.Buffers[i]->GetSize(), false, particles.States[i]);
            graph.BindImportedBuffer(handles[i], particles.Buffers[i]);
        }

        graph.AddPass<ParticleSimulateData>(
            "ParticleSimulate",
            [&](RGPassBuilder& pass, ParticleSimulateData& data)
            {
                for (RGBuffer& buffer : handles)
                    buffer = pass.WriteBuffer(buffer);
                pass.SetSideEffect();
                data.Particles = &particles;
                data.Services  = &services;
            },
            [](ParticleSimulateData& data, RHI::IRHICommandList& cmd)
            {
                const GpuScope scope(cmd, "Particles");
                RecordParticleSimulation(data, cmd);
            });
        return { handles[static_cast<size_t>(ParticleBuffer::State)], handles[static_cast<size_t>(ParticleBuffer::AliveOut)],
                 handles[static_cast<size_t>(ParticleBuffer::Slots)], handles[static_cast<size_t>(ParticleBuffer::Sort)],
                 handles[static_cast<size_t>(ParticleBuffer::CollisionDepth)] };
    }
}

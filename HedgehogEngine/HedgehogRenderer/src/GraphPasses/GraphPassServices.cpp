#include "GraphPassServices.hpp"

#include "Pipeline/PipelineLoader.hpp"
#include "Pipeline/ShaderLoader.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/IRHIDescriptor.hpp"
#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHIPipeline.hpp"

#include <cassert>

namespace Renderer
{
    namespace
    {
        constexpr const char* DEPTH_PREPASS_SHADER = "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/DepthPrepass.shader";
        constexpr const char* SHADOW_SHADER        = "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/ShadowmapPass.shader";

        // The depth format every engine graph asset declares for depth and shadow maps.
        constexpr RHI::Format DEPTH_FORMAT = RHI::Format::D32Float;

        // A depth-only pipeline for dynamic rendering, sharing one viewProj descriptor layout.
        std::unique_ptr<RHI::IRHIPipeline> CreateDepthOnlyPipeline(RHI::IRHIDevice& device, const char* shaderPath,
                                                                   const FS::FileSystemManager& fileSystem,
                                                                   const RHI::IRHIDescriptorSetLayout& layout)
        {
            const ShaderPipelineDesc shader = ShaderLoader::Load(device, shaderPath, fileSystem);
            RHI::GraphicsPipelineDesc desc = shader.Pipeline;
            desc.DescriptorSetLayouts  = { &layout };
            desc.RenderPass            = nullptr;
            desc.ColorAttachmentFormats.clear();
            desc.DepthAttachmentFormat = DEPTH_FORMAT;
            return device.CreateGraphicsPipeline(desc);
        }
    }

    GraphPassServices::GraphPassServices(RHI::IRHIDevice& device, const FS::FileSystemManager& fileSystem)
    {
        // Both depth-only shaders declare the same set 0: one viewProj uniform buffer.
        const ShaderPipelineDesc layoutSource = ShaderLoader::Load(device, DEPTH_PREPASS_SHADER, fileSystem);
        assert(!layoutSource.Layout.DescriptorSets.empty());
        const auto& setLayout = layoutSource.Layout.DescriptorSets[0];
        m_ViewProjLayout = device.CreateDescriptorSetLayout(setLayout);

        const uint32_t totalSets = UNIFORMS_PER_FRAME * HedgehogEngine::MAX_FRAMES_IN_FLIGHT;
        m_Pool = device.CreateDescriptorPool(totalSets, PipelineLoader::MakePoolSizes(setLayout, totalSets));

        m_DepthPrepassPipeline = CreateDepthOnlyPipeline(device, DEPTH_PREPASS_SHADER, fileSystem, *m_ViewProjLayout);
        m_ShadowPipeline       = CreateDepthOnlyPipeline(device, SHADOW_SHADER, fileSystem, *m_ViewProjLayout);

        m_Uniforms.resize(HedgehogEngine::MAX_FRAMES_IN_FLIGHT);
        for (auto& frame : m_Uniforms)
        {
            frame.reserve(UNIFORMS_PER_FRAME);
            for (uint32_t i = 0; i < UNIFORMS_PER_FRAME; ++i)
            {
                UniformSlot slot;
                slot.Buffer = device.CreateBuffer(sizeof(float) * 16, RHI::BufferUsage::UniformBuffer,
                                                  RHI::MemoryUsage::CpuToGpu);
                slot.Set = device.AllocateDescriptorSet(*m_Pool, *m_ViewProjLayout);
                slot.Set->WriteUniformBuffer(0, *slot.Buffer);
                slot.Set->Flush();
                frame.push_back(std::move(slot));
            }
        }
    }

    // The owner waits for the device to go idle first, as for every other GPU resource.
    GraphPassServices::~GraphPassServices() = default;

    void GraphPassServices::BeginFrame(uint32_t frameIndex)
    {
        assert(frameIndex < m_Uniforms.size() && "GraphPassServices::BeginFrame: frame index out of range.");
        m_FrameIndex = frameIndex;
        m_NextSlot   = 0;
    }

    const RHI::IRHIPipeline& GraphPassServices::GetPipeline(EnginePipeline pipeline) const
    {
        switch (pipeline)
        {
            case EnginePipeline::DepthPrepass: return *m_DepthPrepassPipeline;
            case EnginePipeline::Shadow:       return *m_ShadowPipeline;
        }
        assert(false && "GraphPassServices::GetPipeline: unknown pipeline.");
        return *m_DepthPrepassPipeline;
    }

    const RHI::IRHIDescriptorSet& GraphPassServices::AllocateViewProjUniform(const HM::Matrix4x4& viewProj)
    {
        assert(m_NextSlot < UNIFORMS_PER_FRAME && "GraphPassServices: out of viewProj uniforms for this frame.");
        UniformSlot& slot = m_Uniforms[m_FrameIndex][m_NextSlot++];
        slot.Buffer->CopyData(viewProj.GetBuffer(), sizeof(float) * 16);
        return *slot.Set;
    }
}

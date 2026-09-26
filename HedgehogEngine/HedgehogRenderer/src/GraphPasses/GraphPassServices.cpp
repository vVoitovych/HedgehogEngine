#include "GraphPassServices.hpp"

#include "Pipeline/PipelineLoader.hpp"
#include "Pipeline/ShaderLoader.hpp"
#include "ResourceRegistry/ResourceRegistry.hpp"

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
        constexpr const char* FORWARD_SHADER       = "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/GraphForward.shader";
        constexpr const char* GIZMO_SHADER         = "engine://HedgehogEngine/HedgehogRenderer/assets/Shaders/Gizmo.shader";

        // The formats every engine graph asset declares: D32Float depth and shadow maps, and a
        // R16G16B16A16Unorm colour output for the scene and game views.
        constexpr RHI::Format DEPTH_FORMAT = RHI::Format::D32Float;
        constexpr RHI::Format COLOR_FORMAT = RHI::Format::R16G16B16A16Unorm;

        // The twelve edges of the unit cube [0, 1]^3, two vertices each, as GetGizmoBoxLines hands out.
        constexpr float GIZMO_BOX_LINES[GIZMO_BOX_LINE_VERTICES][3] = {
            { 0, 0, 0 }, { 1, 0, 0 },  { 1, 0, 0 }, { 1, 1, 0 },  { 1, 1, 0 }, { 0, 1, 0 },  { 0, 1, 0 }, { 0, 0, 0 },
            { 0, 0, 1 }, { 1, 0, 1 },  { 1, 0, 1 }, { 1, 1, 1 },  { 1, 1, 1 }, { 0, 1, 1 },  { 0, 1, 1 }, { 0, 0, 1 },
            { 0, 0, 0 }, { 0, 0, 1 },  { 1, 0, 0 }, { 1, 0, 1 },  { 1, 1, 0 }, { 1, 1, 1 },  { 0, 1, 0 }, { 0, 1, 1 },
        };

        // A pipeline for dynamic rendering from a .shader file, with the given set layouts.
        std::unique_ptr<RHI::IRHIPipeline> CreatePipeline(RHI::IRHIDevice& device, const ShaderPipelineDesc& shader,
                                                          std::vector<const RHI::IRHIDescriptorSetLayout*> layouts,
                                                          std::vector<RHI::Format> colorFormats,
                                                          RHI::CullMode cullMode)
        {
            RHI::GraphicsPipelineDesc desc = shader.Pipeline;
            desc.DescriptorSetLayouts   = std::move(layouts);
            desc.RenderPass             = nullptr;
            desc.ColorAttachmentFormats = std::move(colorFormats);
            desc.DepthAttachmentFormat  = DEPTH_FORMAT;
            desc.CullMode               = cullMode;
            return device.CreateGraphicsPipeline(desc);
        }
    }

    GraphPassServices::GraphPassServices(RHI::IRHIDevice& device, const FS::FileSystemManager& fileSystem)
    {
        const ShaderPipelineDesc depthShader   = ShaderLoader::Load(device, DEPTH_PREPASS_SHADER, fileSystem);
        const ShaderPipelineDesc shadowShader  = ShaderLoader::Load(device, SHADOW_SHADER, fileSystem);
        const ShaderPipelineDesc forwardShader = ShaderLoader::Load(device, FORWARD_SHADER, fileSystem);
        const ShaderPipelineDesc gizmoShader   = ShaderLoader::Load(device, GIZMO_SHADER, fileSystem);
        assert(!depthShader.Layout.DescriptorSets.empty() && forwardShader.Layout.DescriptorSets.size() >= 3);

        // Both depth-only shaders and the gizmo shader declare the same set 0: one viewProj uniform buffer.
        CreateRing(device, m_ViewProjRing, depthShader.Layout.DescriptorSets[0], UNIFORMS_PER_FRAME, sizeof(float) * 16);
        CreateRing(device, m_ForwardRing, forwardShader.Layout.DescriptorSets[0], FORWARD_UNIFORMS_PER_FRAME,
                   sizeof(ForwardViewUniform));
        CreateRing(device, m_SceneLightsRing, forwardShader.Layout.DescriptorSets[2], SCENE_LIGHTS_PER_FRAME,
                   sizeof(SceneLightsUniform));
        m_MaterialLayout    = device.CreateDescriptorSetLayout(forwardShader.Layout.DescriptorSets[1]);
        m_MaterialPoolSizes = PipelineLoader::MakePoolSizes(forwardShader.Layout.DescriptorSets[1],
                                                            HedgehogEngine::MAX_MATERIAL_COUNT);

        const std::vector<const RHI::IRHIDescriptorSetLayout*> forwardLayouts = {
            m_ForwardRing.Layout.get(), m_MaterialLayout.get(), m_SceneLightsRing.Layout.get() };

        const RHI::CullMode depthCull = depthShader.Pipeline.CullMode;
        m_DepthPrepassPipeline = CreatePipeline(device, depthShader, { m_ViewProjRing.Layout.get() }, {}, depthCull);
        m_ShadowPipeline       = CreatePipeline(device, shadowShader, { m_ViewProjRing.Layout.get() }, {},
                                                shadowShader.Pipeline.CullMode);
        m_ForwardPipeline      = CreatePipeline(device, forwardShader, forwardLayouts, { COLOR_FORMAT },
                                                forwardShader.Pipeline.CullMode);
        m_ForwardDoubleSidedPipeline = CreatePipeline(device, forwardShader, forwardLayouts, { COLOR_FORMAT },
                                                      RHI::CullMode::None);
        m_GizmoPipeline = CreatePipeline(device, gizmoShader, { m_ViewProjRing.Layout.get() }, { COLOR_FORMAT },
                                         gizmoShader.Pipeline.CullMode);

        m_GizmoBoxLines = device.CreateBuffer(sizeof(GIZMO_BOX_LINES), RHI::BufferUsage::VertexBuffer,
                                              RHI::MemoryUsage::CpuToGpu);
        m_GizmoBoxLines->CopyData(GIZMO_BOX_LINES, sizeof(GIZMO_BOX_LINES));
    }

    // The owner waits for the device to go idle first, as for every other GPU resource.
    GraphPassServices::~GraphPassServices() = default;

    void GraphPassServices::CreateRing(RHI::IRHIDevice& device, UniformRing& ring,
                                       const std::vector<RHI::DescriptorBinding>& layout,
                                       uint32_t slotsPerFrame, size_t uniformSize)
    {
        const uint32_t totalSets = slotsPerFrame * HedgehogEngine::MAX_FRAMES_IN_FLIGHT;
        ring.Layout = device.CreateDescriptorSetLayout(layout);
        ring.Pool   = device.CreateDescriptorPool(totalSets, PipelineLoader::MakePoolSizes(layout, totalSets));

        ring.Slots.resize(HedgehogEngine::MAX_FRAMES_IN_FLIGHT);
        for (auto& frame : ring.Slots)
        {
            frame.reserve(slotsPerFrame);
            for (uint32_t i = 0; i < slotsPerFrame; ++i)
            {
                UniformSlot slot;
                slot.Buffer = device.CreateBuffer(uniformSize, RHI::BufferUsage::UniformBuffer, RHI::MemoryUsage::CpuToGpu);
                slot.Set    = device.AllocateDescriptorSet(*ring.Pool, *ring.Layout);
                slot.Set->WriteUniformBuffer(0, *slot.Buffer);
                slot.Set->Flush();
                frame.push_back(std::move(slot));
            }
        }
    }

    void GraphPassServices::BeginFrame(uint32_t frameIndex)
    {
        assert(frameIndex < HedgehogEngine::MAX_FRAMES_IN_FLIGHT && "GraphPassServices::BeginFrame: frame index out of range.");
        m_FrameIndex           = frameIndex;
        m_ViewProjRing.Next    = 0;
        m_ForwardRing.Next     = 0;
        m_SceneLightsRing.Next = 0;
    }

    void GraphPassServices::ProvideMaterialLayout(RHI::IRHIDevice& device, HR::ResourceRegistry& registry) const
    {
        registry.SetMaterialLayout(device, *m_MaterialLayout, HedgehogEngine::MAX_MATERIAL_COUNT, m_MaterialPoolSizes);
    }

    const RHI::IRHIPipeline& GraphPassServices::GetPipeline(EnginePipeline pipeline) const
    {
        switch (pipeline)
        {
            case EnginePipeline::DepthPrepass:       return *m_DepthPrepassPipeline;
            case EnginePipeline::Shadow:             return *m_ShadowPipeline;
            case EnginePipeline::Forward:            return *m_ForwardPipeline;
            case EnginePipeline::ForwardDoubleSided: return *m_ForwardDoubleSidedPipeline;
            case EnginePipeline::Gizmo:              return *m_GizmoPipeline;
        }
        assert(false && "GraphPassServices::GetPipeline: unknown pipeline.");
        return *m_DepthPrepassPipeline;
    }

    RHI::IRHIBuffer& GraphPassServices::GetGizmoBoxLines()
    {
        return *m_GizmoBoxLines;
    }

    const RHI::IRHIDescriptorSet& GraphPassServices::Allocate(UniformRing& ring, const void* data, size_t size)
    {
        assert(ring.Next < ring.Slots[m_FrameIndex].size() && "GraphPassServices: out of uniforms for this frame.");
        UniformSlot& slot = ring.Slots[m_FrameIndex][ring.Next++];
        slot.Buffer->CopyData(data, size);
        return *slot.Set;
    }

    const RHI::IRHIDescriptorSet& GraphPassServices::AllocateViewProjUniform(const HM::Matrix4x4& viewProj)
    {
        return Allocate(m_ViewProjRing, viewProj.GetBuffer(), sizeof(float) * 16);
    }

    const RHI::IRHIDescriptorSet& GraphPassServices::AllocateForwardViewUniform(const ForwardViewUniform& uniform)
    {
        return Allocate(m_ForwardRing, &uniform, sizeof(uniform));
    }

    const RHI::IRHIDescriptorSet& GraphPassServices::AllocateSceneLightsUniform(const SceneLightsUniform& uniform)
    {
        return Allocate(m_SceneLightsRing, &uniform, sizeof(uniform));
    }
}

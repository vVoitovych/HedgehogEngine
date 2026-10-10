#include "GraphPassServices.hpp"

#include "Pipeline/PipelineLoader.hpp"
#include "Pipeline/ShaderLoader.hpp"
#include "ResourceRegistry/ResourceRegistry.hpp"

#include "HedgehogRenderer/Graph/ShadowCascades.hpp"

#include "HedgehogCommon/api/EngineRenderAssets.hpp"
#include "HedgehogCommon/api/RendererSettings.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/IRHIDescriptor.hpp"
#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHIPipeline.hpp"

#include <algorithm>
#include <cassert>

namespace Renderer
{
    namespace
    {
        static_assert(sizeof(HM::Matrix4x4) == 16 * sizeof(float), "The palette uploads matrices as they are.");

        // Where the environment uniform sits in a forward lighting slot's buffer: past the shadow
        // uniform, at a multiple of 256, which every device's uniform offset alignment divides.
        constexpr size_t ENVIRONMENT_UNIFORM_OFFSET = 512;
        static_assert(sizeof(ShadowUniform) <= ENVIRONMENT_UNIFORM_OFFSET, "The shadow uniform overlaps the environment's.");

        // The formats every engine graph asset declares: D32Float depth and shadow maps, the
        // R16G16B16A16Float HDR target the forward pass renders radiance into, and the
        // R16G16B16A16Unorm colour output of the scene and game views, which the ToneMap pass writes
        // and the gizmos and game UI draw over.
        constexpr RHI::Format DEPTH_FORMAT = RHI::Format::D32Float;
        constexpr RHI::Format HDR_FORMAT   = RHI::Format::R16G16B16A16Float;
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
            desc.ColorAttachmentFormats = std::move(colorFormats);
            desc.DepthAttachmentFormat  = DEPTH_FORMAT;
            desc.CullMode               = cullMode;
            return device.CreateGraphicsPipeline(desc);
        }
    }

    GraphPassServices::GraphPassServices(RHI::IRHIDevice& device, const FS::FileSystemManager& fileSystem)
        : m_Device(device)
    {
        const ShaderPipelineDesc depthShader   = ShaderLoader::Load(device, std::string(HedgehogEngine::DEPTH_PREPASS_SHADER), fileSystem);
        const ShaderPipelineDesc shadowShader  = ShaderLoader::Load(device, std::string(HedgehogEngine::SHADOW_SHADER), fileSystem);
        const ShaderPipelineDesc forwardShader = ShaderLoader::Load(device, std::string(HedgehogEngine::FORWARD_SHADER), fileSystem);
        const ShaderPipelineDesc gizmoShader   = ShaderLoader::Load(device, std::string(HedgehogEngine::GIZMO_SHADER), fileSystem);
        const ShaderPipelineDesc depthSkinnedShader   = ShaderLoader::Load(device, std::string(HedgehogEngine::DEPTH_PREPASS_SKINNED_SHADER), fileSystem);
        const ShaderPipelineDesc forwardSkinnedShader = ShaderLoader::Load(device, std::string(HedgehogEngine::FORWARD_SKINNED_SHADER), fileSystem);
        const ShaderPipelineDesc shadowSkinnedShader  = ShaderLoader::Load(device, std::string(HedgehogEngine::SHADOW_SKINNED_SHADER), fileSystem);
        assert(!depthShader.Layout.DescriptorSets.empty() && forwardShader.Layout.DescriptorSets.size() >= 4);
        assert(depthSkinnedShader.Layout.DescriptorSets.size() >= 2 && forwardSkinnedShader.Layout.DescriptorSets.size() >= 5);

        // Both depth-only shaders and the gizmo shader declare the same set 0: one viewProj uniform buffer.
        CreateRing(device, m_ViewProjRing, depthShader.Layout.DescriptorSets[0], UNIFORMS_PER_FRAME, sizeof(float) * 16);
        CreateRing(device, m_ForwardRing, forwardShader.Layout.DescriptorSets[0], FORWARD_UNIFORMS_PER_FRAME,
                   sizeof(ForwardViewUniform));
        CreateRing(device, m_SceneLightsRing, forwardShader.Layout.DescriptorSets[2], SCENE_LIGHTS_PER_FRAME,
                   sizeof(SceneLightsUniform));
        // Set 3's two uniforms share one buffer per slot: the shadow at 0 (binding 0) and the
        // environment at ENVIRONMENT_UNIFORM_OFFSET (binding 2).
        CreateRing(device, m_LightingRing, forwardShader.Layout.DescriptorSets[3], FORWARD_UNIFORMS_PER_FRAME,
                   ENVIRONMENT_UNIFORM_OFFSET + sizeof(EnvironmentUniform));
        for (auto& frame : m_LightingRing.Slots)
        {
            for (UniformSlot& slot : frame)
            {
                slot.Set->WriteUniformBuffer(0, *slot.Buffer, 0, sizeof(ShadowUniform));
                slot.Set->WriteUniformBuffer(2, *slot.Buffer, ENVIRONMENT_UNIFORM_OFFSET, sizeof(EnvironmentUniform));
                slot.Set->Flush();
            }
        }
        RHI::SamplerDesc shadowSampler;
        shadowSampler.AddressModeU  = RHI::AddressMode::ClampToEdge;
        shadowSampler.AddressModeV  = RHI::AddressMode::ClampToEdge;
        shadowSampler.AddressModeW  = RHI::AddressMode::ClampToEdge;
        shadowSampler.MaxAnisotropy = 1.0f;
        shadowSampler.Compare       = RHI::CompareOp::LessOrEqual;
        m_ShadowSampler = device.CreateSampler(shadowSampler);
        m_MaterialLayout    = device.CreateDescriptorSetLayout(forwardShader.Layout.DescriptorSets[1]);
        m_MaterialPoolSizes = PipelineLoader::MakePoolSizes(forwardShader.Layout.DescriptorSets[1],
                                                            HedgehogEngine::MAX_MATERIAL_COUNT);

        const std::vector<const RHI::IRHIDescriptorSetLayout*> forwardLayouts = {
            m_ForwardRing.Layout.get(), m_MaterialLayout.get(), m_SceneLightsRing.Layout.get(), m_LightingRing.Layout.get() };

        const RHI::CullMode depthCull = depthShader.Pipeline.CullMode;
        m_DepthPrepassPipeline = CreatePipeline(device, depthShader, { m_ViewProjRing.Layout.get() }, {}, depthCull);
        m_ShadowPipeline       = CreatePipeline(device, shadowShader, { m_ViewProjRing.Layout.get() }, {},
                                                shadowShader.Pipeline.CullMode);
        m_ForwardPipeline      = CreatePipeline(device, forwardShader, forwardLayouts, { HDR_FORMAT },
                                                forwardShader.Pipeline.CullMode);
        m_ForwardDoubleSidedPipeline = CreatePipeline(device, forwardShader, forwardLayouts, { HDR_FORMAT },
                                                      RHI::CullMode::None);
        m_GizmoPipeline = CreatePipeline(device, gizmoShader, { m_ViewProjRing.Layout.get() }, { COLOR_FORMAT },
                                         gizmoShader.Pipeline.CullMode);
        // The Gizmo pass's lines: the gizmo's layout (viewProj at set 0) over coloured vertices.
        const ShaderPipelineDesc linesShader = ShaderLoader::Load(device, std::string(HedgehogEngine::DEBUG_LINES_SHADER), fileSystem);
        m_DebugLinesPipeline = CreatePipeline(device, linesShader, { m_ViewProjRing.Layout.get() }, { COLOR_FORMAT },
                                              linesShader.Pipeline.CullMode);

        // The depth prepass's and shadow's set 1 and forward's set 4 declare the same binding: one palette set
        // binds to both. One set per frame in flight.
        const std::vector<RHI::DescriptorBinding>& paletteBindings = forwardSkinnedShader.Layout.DescriptorSets[4];
        m_PaletteLayout = device.CreateDescriptorSetLayout(paletteBindings);
        m_PalettePool   = device.CreateDescriptorPool(HedgehogEngine::MAX_FRAMES_IN_FLIGHT,
                                                      PipelineLoader::MakePoolSizes(paletteBindings,
                                                                                    HedgehogEngine::MAX_FRAMES_IN_FLIGHT));
        for (PaletteSlot& palette : m_Palettes)
            palette.Set = device.AllocateDescriptorSet(*m_PalettePool, *m_PaletteLayout);

        std::vector<const RHI::IRHIDescriptorSetLayout*> forwardSkinnedLayouts = forwardLayouts;
        forwardSkinnedLayouts.push_back(m_PaletteLayout.get());
        m_DepthPrepassSkinnedPipeline = CreatePipeline(device, depthSkinnedShader,
                                                       { m_ViewProjRing.Layout.get(), m_PaletteLayout.get() }, {},
                                                       depthSkinnedShader.Pipeline.CullMode);
        m_ShadowSkinnedPipeline = CreatePipeline(device, shadowSkinnedShader,
                                                 { m_ViewProjRing.Layout.get(), m_PaletteLayout.get() }, {},
                                                 shadowSkinnedShader.Pipeline.CullMode);
        m_ForwardSkinnedPipeline = CreatePipeline(device, forwardSkinnedShader, forwardSkinnedLayouts, { HDR_FORMAT },
                                                  forwardSkinnedShader.Pipeline.CullMode);
        m_ForwardSkinnedDoubleSidedPipeline = CreatePipeline(device, forwardSkinnedShader, forwardSkinnedLayouts,
                                                             { HDR_FORMAT }, RHI::CullMode::None);

        // Double-sided materials' depth, and Cutoff materials': the cutoff shaders read the
        // material set the forward pass binds (the same layout object), at set 1, or at set 2 after
        // the palette when skinned.
        m_DepthPrepassDoubleSidedPipeline = CreatePipeline(device, depthShader, { m_ViewProjRing.Layout.get() }, {},
                                                           RHI::CullMode::None);
        m_DepthPrepassSkinnedDoubleSidedPipeline = CreatePipeline(
            device, depthSkinnedShader, { m_ViewProjRing.Layout.get(), m_PaletteLayout.get() }, {}, RHI::CullMode::None);
        const ShaderPipelineDesc cutoffShader =
            ShaderLoader::Load(device, std::string(HedgehogEngine::DEPTH_PREPASS_CUTOFF_SHADER), fileSystem);
        const ShaderPipelineDesc cutoffSkinnedShader =
            ShaderLoader::Load(device, std::string(HedgehogEngine::DEPTH_PREPASS_CUTOFF_SKINNED_SHADER), fileSystem);
        const std::vector<const RHI::IRHIDescriptorSetLayout*> cutoffLayouts = { m_ViewProjRing.Layout.get(),
                                                                                 m_MaterialLayout.get() };
        const std::vector<const RHI::IRHIDescriptorSetLayout*> cutoffSkinnedLayouts = {
            m_ViewProjRing.Layout.get(), m_PaletteLayout.get(), m_MaterialLayout.get() };
        m_DepthPrepassCutoffPipeline = CreatePipeline(device, cutoffShader, cutoffLayouts, {}, cutoffShader.Pipeline.CullMode);
        m_DepthPrepassCutoffDoubleSidedPipeline = CreatePipeline(device, cutoffShader, cutoffLayouts, {}, RHI::CullMode::None);
        m_DepthPrepassCutoffSkinnedPipeline = CreatePipeline(device, cutoffSkinnedShader, cutoffSkinnedLayouts, {},
                                                             cutoffSkinnedShader.Pipeline.CullMode);
        m_DepthPrepassCutoffSkinnedDoubleSidedPipeline = CreatePipeline(device, cutoffSkinnedShader, cutoffSkinnedLayouts,
                                                                        {}, RHI::CullMode::None);

        // The shadow's twins and alpha-tested casters, through the same layouts as the depth prepass's.
        m_ShadowDoubleSidedPipeline = CreatePipeline(device, shadowShader, { m_ViewProjRing.Layout.get() }, {},
                                                     RHI::CullMode::None);
        m_ShadowSkinnedDoubleSidedPipeline = CreatePipeline(
            device, shadowSkinnedShader, { m_ViewProjRing.Layout.get(), m_PaletteLayout.get() }, {}, RHI::CullMode::None);
        const ShaderPipelineDesc shadowCutoffShader =
            ShaderLoader::Load(device, std::string(HedgehogEngine::SHADOW_CUTOFF_SHADER), fileSystem);
        const ShaderPipelineDesc shadowCutoffSkinnedShader =
            ShaderLoader::Load(device, std::string(HedgehogEngine::SHADOW_CUTOFF_SKINNED_SHADER), fileSystem);
        m_ShadowCutoffPipeline = CreatePipeline(device, shadowCutoffShader, cutoffLayouts, {},
                                                shadowCutoffShader.Pipeline.CullMode);
        m_ShadowCutoffDoubleSidedPipeline = CreatePipeline(device, shadowCutoffShader, cutoffLayouts, {},
                                                           RHI::CullMode::None);
        m_ShadowCutoffSkinnedPipeline = CreatePipeline(device, shadowCutoffSkinnedShader, cutoffSkinnedLayouts, {},
                                                       shadowCutoffSkinnedShader.Pipeline.CullMode);
        m_ShadowCutoffSkinnedDoubleSidedPipeline = CreatePipeline(device, shadowCutoffSkinnedShader, cutoffSkinnedLayouts,
                                                                  {}, RHI::CullMode::None);

        // Transparent materials, blended through the forward layouts; their back-face twins cull
        // the front faces, for a double-sided one's far side.
        const ShaderPipelineDesc transparentShader =
            ShaderLoader::Load(device, std::string(HedgehogEngine::FORWARD_TRANSPARENT_SHADER), fileSystem);
        const ShaderPipelineDesc transparentSkinnedShader =
            ShaderLoader::Load(device, std::string(HedgehogEngine::FORWARD_TRANSPARENT_SKINNED_SHADER), fileSystem);
        m_ForwardTransparentPipeline = CreatePipeline(device, transparentShader, forwardLayouts, { HDR_FORMAT },
                                                      transparentShader.Pipeline.CullMode);
        m_ForwardTransparentBackFacesPipeline = CreatePipeline(device, transparentShader, forwardLayouts, { HDR_FORMAT },
                                                               RHI::CullMode::Front);
        m_ForwardTransparentSkinnedPipeline = CreatePipeline(device, transparentSkinnedShader, forwardSkinnedLayouts,
                                                             { HDR_FORMAT }, transparentSkinnedShader.Pipeline.CullMode);
        m_ForwardTransparentSkinnedBackFacesPipeline = CreatePipeline(device, transparentSkinnedShader,
                                                                      forwardSkinnedLayouts, { HDR_FORMAT },
                                                                      RHI::CullMode::Front);

        // The editor's selection mask: one R8 colour target and no depth attachment, through the
        // depth prepass's layouts (viewProj at set 0, the palette at set 1 when skinned).
        const auto createMaskPipeline = [&](std::string_view path,
                                            std::vector<const RHI::IRHIDescriptorSetLayout*> layouts)
        {
            const ShaderPipelineDesc  shader = ShaderLoader::Load(device, std::string(path), fileSystem);
            RHI::GraphicsPipelineDesc desc   = shader.Pipeline;
            desc.DescriptorSetLayouts   = std::move(layouts);
            desc.ColorAttachmentFormats = { RHI::Format::R8Unorm };
            desc.DepthAttachmentFormat  = RHI::Format::Undefined;
            return device.CreateGraphicsPipeline(desc);
        };
        m_SelectionMaskPipeline = createMaskPipeline(HedgehogEngine::SELECTION_MASK_SHADER, { m_ViewProjRing.Layout.get() });
        m_SelectionMaskSkinnedPipeline = createMaskPipeline(HedgehogEngine::SELECTION_MASK_SKINNED_SHADER,
                                                            { m_ViewProjRing.Layout.get(), m_PaletteLayout.get() });

        // The game UI draws into the colour target alone: no depth attachment.
        const ShaderPipelineDesc gameUiShader = ShaderLoader::Load(device, std::string(HedgehogEngine::GAME_UI_SHADER), fileSystem);
        assert(!gameUiShader.Layout.DescriptorSets.empty());
        m_UiTextureLayout = device.CreateDescriptorSetLayout(gameUiShader.Layout.DescriptorSets[0]);
        RHI::GraphicsPipelineDesc gameUiDesc = gameUiShader.Pipeline;
        gameUiDesc.DescriptorSetLayouts   = { m_UiTextureLayout.get() };
        gameUiDesc.ColorAttachmentFormats = { COLOR_FORMAT };
        gameUiDesc.DepthAttachmentFormat  = RHI::Format::Undefined;
        m_GameUiPipeline = device.CreateGraphicsPipeline(gameUiDesc);

        // The tone map: the HDR target sampled through a ring of sets, onto the colour output with
        // no depth attachment.
        const ShaderPipelineDesc toneMapShader = ShaderLoader::Load(device, std::string(HedgehogEngine::TONE_MAP_SHADER), fileSystem);
        assert(!toneMapShader.Layout.DescriptorSets.empty());
        const std::vector<RHI::DescriptorBinding>& sampledBindings = toneMapShader.Layout.DescriptorSets[0];
        const uint32_t sampledSets = SAMPLED_TEXTURES_PER_FRAME * HedgehogEngine::MAX_FRAMES_IN_FLIGHT;
        m_SampledTextureLayout = device.CreateDescriptorSetLayout(sampledBindings);
        m_SampledTexturePool   = device.CreateDescriptorPool(sampledSets, PipelineLoader::MakePoolSizes(sampledBindings, sampledSets));
        m_SampledTextureSets.resize(HedgehogEngine::MAX_FRAMES_IN_FLIGHT);
        for (auto& frame : m_SampledTextureSets)
        {
            for (uint32_t i = 0; i < SAMPLED_TEXTURES_PER_FRAME; ++i)
                frame.push_back(device.AllocateDescriptorSet(*m_SampledTexturePool, *m_SampledTextureLayout));
        }
        RHI::SamplerDesc clamp;
        clamp.AddressModeU   = RHI::AddressMode::ClampToEdge;
        clamp.AddressModeV   = RHI::AddressMode::ClampToEdge;
        clamp.AddressModeW   = RHI::AddressMode::ClampToEdge;
        clamp.MaxAnisotropy  = 1.0f;
        m_LinearClampSampler = device.CreateSampler(clamp);

        RHI::GraphicsPipelineDesc toneMapDesc = toneMapShader.Pipeline;
        toneMapDesc.DescriptorSetLayouts   = { m_SampledTextureLayout.get() };
        toneMapDesc.ColorAttachmentFormats = { COLOR_FORMAT };
        toneMapDesc.DepthAttachmentFormat  = RHI::Format::Undefined;
        m_ToneMapPipeline = device.CreateGraphicsPipeline(toneMapDesc);

        // The skybox: the radiance cube through the same sampled-texture sets (one combined image
        // sampler at set 0), into the HDR target against the view's depth.
        const ShaderPipelineDesc skyboxShader = ShaderLoader::Load(device, std::string(HedgehogEngine::SKYBOX_SHADER), fileSystem);
        RHI::GraphicsPipelineDesc skyboxDesc = skyboxShader.Pipeline;
        skyboxDesc.DescriptorSetLayouts   = { m_SampledTextureLayout.get() };
        skyboxDesc.ColorAttachmentFormats = { HDR_FORMAT };
        skyboxDesc.DepthAttachmentFormat  = DEPTH_FORMAT;
        m_SkyboxPipeline = device.CreateGraphicsPipeline(skyboxDesc);

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
        m_LightingRing.Next    = 0;
        m_NextSampledTexture   = 0;
    }

    void GraphPassServices::ProvideMaterialLayout(RHI::IRHIDevice& device, HR::ResourceRegistry& registry) const
    {
        registry.SetMaterialLayout(device, *m_MaterialLayout, HedgehogEngine::MAX_MATERIAL_COUNT, m_MaterialPoolSizes);
    }

    void GraphPassServices::ProvideUiTextureLayout(HR::ResourceRegistry& registry) const
    {
        registry.SetUiTextureLayout(m_Device, *m_UiTextureLayout);
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
            case EnginePipeline::DepthPrepassSkinned:       return *m_DepthPrepassSkinnedPipeline;
            case EnginePipeline::ForwardSkinned:            return *m_ForwardSkinnedPipeline;
            case EnginePipeline::ForwardSkinnedDoubleSided: return *m_ForwardSkinnedDoubleSidedPipeline;
            case EnginePipeline::ShadowSkinned:             return *m_ShadowSkinnedPipeline;
            case EnginePipeline::GameUi:                    return *m_GameUiPipeline;
            case EnginePipeline::DebugLines:                return *m_DebugLinesPipeline;
            case EnginePipeline::ToneMap:                   return *m_ToneMapPipeline;
            case EnginePipeline::Skybox:                    return *m_SkyboxPipeline;
            case EnginePipeline::DepthPrepassDoubleSided:              return *m_DepthPrepassDoubleSidedPipeline;
            case EnginePipeline::DepthPrepassSkinnedDoubleSided:       return *m_DepthPrepassSkinnedDoubleSidedPipeline;
            case EnginePipeline::DepthPrepassCutoff:                   return *m_DepthPrepassCutoffPipeline;
            case EnginePipeline::DepthPrepassCutoffDoubleSided:        return *m_DepthPrepassCutoffDoubleSidedPipeline;
            case EnginePipeline::DepthPrepassCutoffSkinned:            return *m_DepthPrepassCutoffSkinnedPipeline;
            case EnginePipeline::DepthPrepassCutoffSkinnedDoubleSided: return *m_DepthPrepassCutoffSkinnedDoubleSidedPipeline;
            case EnginePipeline::ShadowDoubleSided:                    return *m_ShadowDoubleSidedPipeline;
            case EnginePipeline::ShadowSkinnedDoubleSided:             return *m_ShadowSkinnedDoubleSidedPipeline;
            case EnginePipeline::ShadowCutoff:                         return *m_ShadowCutoffPipeline;
            case EnginePipeline::ShadowCutoffDoubleSided:              return *m_ShadowCutoffDoubleSidedPipeline;
            case EnginePipeline::ShadowCutoffSkinned:                  return *m_ShadowCutoffSkinnedPipeline;
            case EnginePipeline::ShadowCutoffSkinnedDoubleSided:       return *m_ShadowCutoffSkinnedDoubleSidedPipeline;
            case EnginePipeline::ForwardTransparent:                   return *m_ForwardTransparentPipeline;
            case EnginePipeline::ForwardTransparentBackFaces:          return *m_ForwardTransparentBackFacesPipeline;
            case EnginePipeline::ForwardTransparentSkinned:            return *m_ForwardTransparentSkinnedPipeline;
            case EnginePipeline::ForwardTransparentSkinnedBackFaces:   return *m_ForwardTransparentSkinnedBackFacesPipeline;
            case EnginePipeline::SelectionMask:                        return *m_SelectionMaskPipeline;
            case EnginePipeline::SelectionMaskSkinned:                 return *m_SelectionMaskSkinnedPipeline;
        }
        assert(false && "GraphPassServices::GetPipeline: unknown pipeline.");
        return *m_DepthPrepassPipeline;
    }

    RHI::IRHIBuffer& GraphPassServices::GetGizmoBoxLines()
    {
        return *m_GizmoBoxLines;
    }

    RHI::IRHIDescriptorSet& GraphPassServices::Allocate(UniformRing& ring, const void* data, size_t size)
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

    const RHI::IRHIDescriptorSet& GraphPassServices::AllocateForwardLighting(const ShadowUniform& shadow,
                                                                             const RHI::IRHITexture& shadowAtlas,
                                                                             const ForwardEnvironment& environment)
    {
        assert(environment.Radiance && environment.BrdfLut && "Forward: the frame has no environment textures.");
        // The uniforms are copied; the atlas is this frame's graph transient and the environment may
        // change, so the textures are written into the set each time (this slot's fence has
        // signaled, so the set is no longer read).
        assert(m_LightingRing.Next < m_LightingRing.Slots[m_FrameIndex].size() && "GraphPassServices: out of forward lighting sets.");
        UniformSlot& slot = m_LightingRing.Slots[m_FrameIndex][m_LightingRing.Next++];
        slot.Buffer->CopyData(&shadow, sizeof(shadow));
        slot.Buffer->CopyData(&environment.Uniform, sizeof(environment.Uniform), ENVIRONMENT_UNIFORM_OFFSET);
        RHI::IRHIDescriptorSet& set = *slot.Set;
        set.WriteTexture(1, shadowAtlas, *m_ShadowSampler);
        set.WriteTexture(3, *environment.Radiance, *m_LinearClampSampler);
        set.WriteTexture(4, *environment.BrdfLut, *m_LinearClampSampler);
        set.Flush();
        return set;
    }

    const RHI::IRHIDescriptorSet& GraphPassServices::AllocateSampledTexture(const RHI::IRHITexture& texture)
    {
        std::vector<std::unique_ptr<RHI::IRHIDescriptorSet>>& sets = m_SampledTextureSets[m_FrameIndex];
        assert(m_NextSampledTexture < sets.size() && "GraphPassServices: out of sampled-texture sets for this frame.");
        // This slot's fence has signaled, so the set is no longer read and may be rewritten.
        RHI::IRHIDescriptorSet& set = *sets[m_NextSampledTexture++];
        set.WriteTexture(0, texture, *m_LinearClampSampler);
        set.Flush();
        return set;
    }

    const RHI::IRHIDescriptorSet* GraphPassServices::UploadJointPalette(std::span<const HM::Matrix4x4> matrices)
    {
        if (matrices.empty())
            return nullptr;

        // This slot's fence has signaled, so its buffer is no longer read and may be replaced.
        PaletteSlot& palette = m_Palettes[m_FrameIndex];
        if (matrices.size() > palette.Capacity)
        {
            palette.Capacity = std::max({ matrices.size(), palette.Capacity * 2, MIN_PALETTE_CAPACITY });
            palette.Buffer   = m_Device.CreateBuffer(palette.Capacity * sizeof(HM::Matrix4x4),
                                                     RHI::BufferUsage::StorageBuffer, RHI::MemoryUsage::CpuToGpu);
            palette.Set->WriteStorageBuffer(0, *palette.Buffer);
            palette.Set->Flush();
        }
        palette.Buffer->CopyData(matrices.data(), matrices.size_bytes());
        return palette.Set.get();
    }

    GraphPassServices::UiGeometry GraphPassServices::UploadUiGeometry(const HX::UiDrawList& list)
    {
        if (list.Indices.empty())
            return {};

        // This slot's fence has signaled, so its buffers are no longer read and may be replaced.
        UiGeometrySlot& slot = m_UiGeometry[m_FrameIndex];
        if (list.Vertices.size() > slot.VertexCapacity)
        {
            slot.VertexCapacity = std::max({ list.Vertices.size(), slot.VertexCapacity * 2, MIN_UI_VERTEX_CAPACITY });
            slot.Vertices = m_Device.CreateBuffer(slot.VertexCapacity * sizeof(HX::UiVertex),
                                                  RHI::BufferUsage::VertexBuffer, RHI::MemoryUsage::CpuToGpu);
        }
        if (list.Indices.size() > slot.IndexCapacity)
        {
            slot.IndexCapacity = std::max({ list.Indices.size(), slot.IndexCapacity * 2, MIN_UI_INDEX_CAPACITY });
            slot.Indices = m_Device.CreateBuffer(slot.IndexCapacity * sizeof(uint16_t), RHI::BufferUsage::IndexBuffer,
                                                 RHI::MemoryUsage::CpuToGpu);
        }
        slot.Vertices->CopyData(list.Vertices.data(), list.Vertices.size() * sizeof(HX::UiVertex));
        slot.Indices->CopyData(list.Indices.data(), list.Indices.size() * sizeof(uint16_t));
        return { slot.Vertices.get(), slot.Indices.get() };
    }

    RHI::IRHIBuffer* GraphPassServices::UploadDebugLines(std::span<const HX::DebugLineVertex> vertices)
    {
        if (vertices.empty())
            return nullptr;

        // This slot's fence has signaled, so its buffer is no longer read and may be replaced.
        DebugLineSlot& slot = m_DebugLines[m_FrameIndex];
        if (vertices.size() > slot.Capacity)
        {
            slot.Capacity = std::max({ vertices.size(), slot.Capacity * 2, MIN_DEBUG_LINE_VERTEX_CAPACITY });
            slot.Vertices = m_Device.CreateBuffer(slot.Capacity * sizeof(HX::DebugLineVertex),
                                                  RHI::BufferUsage::VertexBuffer, RHI::MemoryUsage::CpuToGpu);
        }
        slot.Vertices->CopyData(vertices.data(), vertices.size() * sizeof(HX::DebugLineVertex));
        return slot.Vertices.get();
    }
}

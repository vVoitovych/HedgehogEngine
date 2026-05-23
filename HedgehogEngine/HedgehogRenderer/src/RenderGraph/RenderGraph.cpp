#include "RenderGraph.hpp"

#include "RenderContext.hpp"
#include "RenderNodes/IRenderNode.hpp"
#include "ResourceManager/ResourceManager.hpp"

#include "HedgehogEngine/api/Frame/FrameData.hpp"
#include "HedgehogSettings/Settings/HedgehogSettings.hpp"
#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

#include "Logger/api/Logger.hpp"

#include <cassert>
#include <unordered_map>
#include <utility>

namespace Renderer
{
    void RenderGraph::AddNode(IRenderNode* node)
    {
        assert(node && "RenderGraph::AddNode: null node");
        m_Nodes.push_back(node);
    }

    void RenderGraph::Compile(RHI::IRHIDevice& device, const ResourceManager& resourceManager)
    {
        assert(!m_Nodes.empty() && "RenderGraph::Compile() called with no nodes");

        for (IRenderNode* node : m_Nodes)
        {
            node->Setup(*this);
            node->ApplyYAMLBindings();
        }

        const uint32_t backW = resourceManager.GetRHIColorBuffer().GetWidth();
        const uint32_t backH = resourceManager.GetRHIColorBuffer().GetHeight();
        m_GraphResources.CreateResources(device, backW, backH);

        BuildTextureRegistry(resourceManager);
        ValidateBindings();
        BuildBarrierPlan();
    }

    void RenderGraph::BuildTextureRegistry(const ResourceManager& resourceManager)
    {
        m_TextureRegistry.clear();
        m_TextureRegistry["RHIColorBuffer"]   = &resourceManager.GetRHIColorBuffer();
        m_TextureRegistry["RHIDepthBuffer"]   = &resourceManager.GetRHIDepthBuffer();
        m_TextureRegistry["RHIShadowMap"]     = &resourceManager.GetRHIShadowMap();
        m_TextureRegistry["RHIShadowMask"]    = &resourceManager.GetRHIShadowMask();
        m_TextureRegistry["SceneColorBuffer"] = &resourceManager.GetSceneColorBuffer();

        m_GraphResources.PopulateTextureRegistry(m_TextureRegistry);
    }

    void RenderGraph::ValidateBindings() const
    {
        std::unordered_map<std::string, std::string> writers;

        for (const IRenderNode* node : m_Nodes)
        {
            const RenderNodeDesc& desc = node->GetDesc();

            for (const auto& slot : desc.inputs)
            {
                const bool knownTexture = m_TextureRegistry.find(slot.name) != m_TextureRegistry.end();
                const bool knownBuffer  = m_GraphResources.HasBuffer(slot.name);
                if (!knownTexture && !knownBuffer)
                {
                    LOGERROR("RenderGraph: node '", desc.name, "' input '", slot.name,
                             "' references an undeclared resource.");
                }
            }

            for (const auto& slot : desc.outputs)
            {
                const bool knownTexture = m_TextureRegistry.find(slot.name) != m_TextureRegistry.end();
                const bool knownBuffer  = m_GraphResources.HasBuffer(slot.name);
                if (!knownTexture && !knownBuffer)
                {
                    LOGERROR("RenderGraph: node '", desc.name, "' output '", slot.name,
                             "' references an undeclared resource.");
                }

                auto [it, inserted] = writers.emplace(slot.name, desc.name);
                if (!inserted)
                {
                    LOGWARNING("RenderGraph: resource '", slot.name, "' written by both '",
                               it->second, "' and '", desc.name,
                               "' — resource aliasing is not yet supported.");
                }
            }
        }
    }

    void RenderGraph::BuildBarrierPlan()
    {
        m_BarrierPlan.assign(m_Nodes.size(), {});

        // ── Pass 1: wrap-around barriers ────────────────────────────────────────
        // For every resource that is written and then read within the same frame,
        // the image ends up in a READ layout at the end of frame N.  On frame N+1
        // the barrier plan would emit "from writeLayout → readLayout" again, but the
        // actual current layout is readLayout → Vulkan validation error.
        //
        // Fix: insert a barrier before the first writer that transitions from
        // VK_IMAGE_LAYOUT_UNDEFINED → writeLayout.  Using Undefined as srcLayout
        // is always valid per the Vulkan spec ("don't care about previous contents")
        // and handles both the very first frame (image starts in Undefined) and all
        // subsequent frames (discards the leftover read layout without a mismatch).

        struct ResourceFrameInfo
        {
            size_t           m_FirstWriteIndex  = SIZE_MAX;
            RHI::ImageLayout m_FirstWriteLayout = RHI::ImageLayout::Undefined;
            size_t           m_LastReadIndex    = SIZE_MAX;
        };
        std::unordered_map<std::string, ResourceFrameInfo> frameInfo;

        for (size_t i = 0; i < m_Nodes.size(); ++i)
        {
            const RenderNodeDesc& desc = m_Nodes[i]->GetDesc();
            for (const auto& slot : desc.outputs)
            {
                auto& info = frameInfo[slot.name];
                if (info.m_FirstWriteIndex == SIZE_MAX)
                {
                    info.m_FirstWriteIndex  = i;
                    info.m_FirstWriteLayout = slot.layout;
                }
            }
            for (const auto& slot : desc.inputs)
                frameInfo[slot.name].m_LastReadIndex = i;
        }

        for (const auto& [name, info] : frameInfo)
        {
            if (info.m_FirstWriteIndex == SIZE_MAX || info.m_LastReadIndex == SIZE_MAX)
                continue; // written-only or read-only: no cross-frame risk
            if (info.m_LastReadIndex <= info.m_FirstWriteIndex)
                continue; // read before write within the frame: unusual, skip

            const auto texIt = m_TextureRegistry.find(name);
            if (texIt == m_TextureRegistry.end())
                continue;

            m_BarrierPlan[info.m_FirstWriteIndex].push_back({
                texIt->second,
                RHI::ImageLayout::Undefined,   // srcLayout: discard previous contents
                info.m_FirstWriteLayout        // dstLayout: required by the writer
            });
        }

        // ── Pass 2: within-frame read-after-write barriers ──────────────────────
        // Track the last node that wrote each resource and the layout it left it in.
        std::unordered_map<std::string, std::pair<size_t, RHI::ImageLayout>> lastWrite;

        for (size_t i = 0; i < m_Nodes.size(); ++i)
        {
            const RenderNodeDesc& desc = m_Nodes[i]->GetDesc();

            // For each declared input: if a prior write exists, plan a transition barrier.
            for (const auto& slot : desc.inputs)
            {
                auto writeIt = lastWrite.find(slot.name);
                auto texIt   = m_TextureRegistry.find(slot.name);
                if (writeIt != lastWrite.end() && texIt != m_TextureRegistry.end())
                {
                    m_BarrierPlan[i].push_back({
                        texIt->second,
                        writeIt->second.second,  // from: producer's post-write layout
                        slot.layout              // to:   consumer's required layout
                    });
                }
            }

            // Record this node's output writes.
            for (const auto& slot : desc.outputs)
                lastWrite[slot.name] = { i, slot.layout };
        }
    }

    void RenderGraph::Execute(RenderContext& ctx)
    {
        auto& cmd = ctx.GetCommandList();
        for (size_t i = 0; i < m_Nodes.size(); ++i)
        {
            for (const auto& barrier : m_BarrierPlan[i])
            {
                cmd.TransitionTexture(
                    const_cast<RHI::IRHITexture&>(*barrier.m_Texture),
                    barrier.m_FromLayout,
                    barrier.m_ToLayout);
            }

            if (m_Nodes[i]->IsEnabled())
                m_Nodes[i]->Execute(ctx);
        }
    }

    void RenderGraph::Cleanup(RHI::IRHIDevice& device)
    {
        for (IRenderNode* node : m_Nodes)
            node->Cleanup(device);
        m_Nodes.clear();
        m_GraphResources.Cleanup();
    }

    void RenderGraph::DeclareGraphTexture(const GraphTextureDesc& desc)
    {
        m_GraphResources.DeclareTexture(desc);
    }

    void RenderGraph::DeclareGraphBuffer(const GraphBufferDesc& desc)
    {
        m_GraphResources.DeclareBuffer(desc);
    }

    const RHI::IRHIBuffer* RenderGraph::GetBuffer(const std::string& name) const
    {
        return m_GraphResources.GetBuffer(name);
    }

    void RenderGraph::BeginFrame()
    {
        for (IRenderNode* node : m_Nodes)
            node->BeginFrame();
    }

    void RenderGraph::DiscardFrame()
    {
        for (IRenderNode* node : m_Nodes)
            node->DiscardFrame();
    }

    void* RenderGraph::GetSceneViewTextureId() const
    {
        for (IRenderNode* node : m_Nodes)
        {
            if (void* id = node->GetSceneViewTextureId())
                return id;
        }
        return nullptr;
    }

    void RenderGraph::UpdateData(const HedgehogEngine::FrameData&   frame,
                                 uint32_t                           frameIndex,
                                 const HedgehogSettings::Settings&  settings)
    {
        for (IRenderNode* node : m_Nodes)
            node->UpdateData(frame, frameIndex, settings);
    }

    void RenderGraph::OnWindowResize(RHI::IRHIDevice& device,
                                     const ResourceManager& resourceManager)
    {
        m_GraphResources.RecreateBackbufferSized(
            device,
            resourceManager.GetRHIColorBuffer().GetWidth(),
            resourceManager.GetRHIColorBuffer().GetHeight());

        for (IRenderNode* node : m_Nodes)
            node->OnWindowResize(device, resourceManager);

        BuildTextureRegistry(resourceManager);
        BuildBarrierPlan();
    }

    void RenderGraph::OnSceneViewResize(RHI::IRHIDevice& device,
                                        const ResourceManager& resourceManager)
    {
        for (IRenderNode* node : m_Nodes)
            node->OnSceneViewResize(device, resourceManager);

        BuildTextureRegistry(resourceManager);
        BuildBarrierPlan();
    }

    void RenderGraph::OnSettingsChanged(RHI::IRHIDevice& device,
                                        const HedgehogSettings::Settings& settings,
                                        const ResourceManager& resourceManager)
    {
        for (IRenderNode* node : m_Nodes)
            node->OnSettingsChanged(device, settings, resourceManager);

        BuildTextureRegistry(resourceManager);
        BuildBarrierPlan();
    }
}

#include "RenderGraph.hpp"

#include "IRenderPass.hpp"
#include "RenderGraphBuilder.hpp"
#include "RenderGraphResourcePool.hpp"

#include "Profiling/FrameStats.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHICommandList.hpp"

#include <algorithm>
#include <cassert>

namespace Renderer
{
    RenderGraph::RenderGraph()
    {
        m_Pool = std::make_unique<RenderGraphResourcePool>();
    }

    RenderGraph::~RenderGraph()
    {
    }

    void RenderGraph::AddPass(IRenderPass* pass)
    {
        assert(pass && "Cannot add a null pass to the render graph.");
        m_PassIndexByPointer[pass] = m_Passes.size();
        m_Passes.push_back(pass);
        m_PassDeps.emplace_back();
    }

    void RenderGraph::AddAndCompilePass(IRenderPass* pass, RHI::IRHIDevice& device)
    {
        const size_t index = m_Passes.size();
        AddPass(pass);

        RenderGraphBuilder builder(*this, index);
        pass->Setup(builder);
        pass->CreateFramebuffers(device, *this);
    }

    void RenderGraph::Compile(RHI::IRHIDevice& device,
                              uint32_t swapchainWidth, uint32_t swapchainHeight,
                              uint32_t sceneViewWidth, uint32_t sceneViewHeight)
    {
        m_Pool->SetSwapchainSize(swapchainWidth, swapchainHeight);
        m_Pool->SetSceneViewSize(sceneViewWidth, sceneViewHeight);

        for (size_t i = 0; i < m_Passes.size(); ++i)
        {
            RenderGraphBuilder builder(*this, i);
            m_Passes[i]->Setup(builder);
        }

        for (const SizeClass sizeClass : { SizeClass::SwapchainRelative, SizeClass::SceneViewRelative, SizeClass::Fixed })
        {
            const auto changed = m_Pool->Recreate(sizeClass, device);
            for (const ResourceHandle handle : changed)
                m_CurrentLayouts[handle] = RHI::ImageLayout::Undefined;
        }

        for (IRenderPass* pass : m_Passes)
            pass->CreateFramebuffers(device, *this);
    }

    void RenderGraph::Update(const RenderGraphContext& ctx)
    {
        for (IRenderPass* pass : m_Passes)
            pass->Update(ctx);
    }

    void RenderGraph::transitionSampledReads(size_t passIndex, RHI::IRHICommandList& cmd)
    {
        for (const ResourceHandle handle : m_PassDeps[passIndex].m_ReadSampled)
        {
            const RHI::ImageLayout oldLayout = m_CurrentLayouts[handle];
            if (oldLayout == RHI::ImageLayout::ShaderReadOnly)
                continue;

            cmd.TransitionTexture(m_Pool->GetTexture(handle), oldLayout, RHI::ImageLayout::ShaderReadOnly);
            m_CurrentLayouts[handle] = RHI::ImageLayout::ShaderReadOnly;
        }
    }

    void RenderGraph::executePassAt(size_t passIndex, RenderGraphContext& ctx, FrameStats& stats)
    {
        assert(ctx.m_CommandList && "RenderGraphContext::m_CommandList must be set before Execute().");

        IRenderPass* pass = m_Passes[passIndex];

        // Tracy's ZoneScopedN (behind HH_PROFILE_ZONE) needs a compile-time string literal, so
        // it can't take pass->GetName() here — each pass's own Execute() opens its Tracy zone
        // with a literal instead (same name). ScopedCpuSample has no such constraint: it just
        // does a name-keyed lookup into FrameStats at runtime.
        ScopedCpuSample sample(stats, pass->GetName());

        transitionSampledReads(passIndex, *ctx.m_CommandList);

        pass->Execute(ctx);

        for (const ResourceHandle handle : m_PassDeps[passIndex].m_Writes)
            m_CurrentLayouts[handle] = m_WriteFinalLayout.at(handle);
    }

    void RenderGraph::Execute(RenderGraphContext& ctx, FrameStats& stats)
    {
        for (size_t i = 0; i < m_Passes.size(); ++i)
            executePassAt(i, ctx, stats);
    }

    void RenderGraph::ExecutePass(IRenderPass* pass, RenderGraphContext& ctx, FrameStats& stats)
    {
        const auto it = m_PassIndexByPointer.find(pass);
        assert(it != m_PassIndexByPointer.end() && "ExecutePass() called on a pass never registered with AddPass/AddAndCompilePass.");
        executePassAt(it->second, ctx, stats);
    }

    void RenderGraph::SetSwapchainSize(uint32_t width, uint32_t height)
    {
        m_Pool->SetSwapchainSize(width, height);
    }

    void RenderGraph::SetSceneViewSize(uint32_t width, uint32_t height)
    {
        m_Pool->SetSceneViewSize(width, height);
    }

    void RenderGraph::SetFixedSize(const std::string& name, uint32_t width, uint32_t height)
    {
        m_Pool->SetFixedSize(resolveHandle(name), width, height);
    }

    void RenderGraph::createFramebuffersForHandles(const std::vector<ResourceHandle>& handles, RHI::IRHIDevice& device)
    {
        for (size_t i = 0; i < m_Passes.size(); ++i)
        {
            const bool dependsOnChange = std::any_of(
                m_PassDeps[i].m_AllHandles.begin(), m_PassDeps[i].m_AllHandles.end(),
                [&handles](ResourceHandle h)
                {
                    return std::find(handles.begin(), handles.end(), h) != handles.end();
                });

            if (dependsOnChange)
                m_Passes[i]->CreateFramebuffers(device, *this);
        }
    }

    void RenderGraph::Invalidate(SizeClass sizeClass, RHI::IRHIDevice& device)
    {
        const auto changed = m_Pool->Recreate(sizeClass, device);
        for (const ResourceHandle handle : changed)
            m_CurrentLayouts[handle] = RHI::ImageLayout::Undefined;

        createFramebuffersForHandles(changed, device);
    }

    RHI::IRHITexture& RenderGraph::GetTexture(ResourceHandle handle) const
    {
        return m_Pool->GetTexture(handle);
    }

    RHI::IRHITexture& RenderGraph::GetTexture(const std::string& name) const
    {
        return m_Pool->GetTexture(resolveHandle(name));
    }

    void RenderGraph::Cleanup(RHI::IRHIDevice& device)
    {
        (void)device;
        m_Pool->Cleanup();
    }

    ResourceHandle RenderGraph::resolveHandle(const std::string& name) const
    {
        const auto it = m_NameToHandle.find(name);
        assert(it != m_NameToHandle.end() && "Reading/writing an undeclared graph resource — the owning pass must run its Setup() first.");
        return it->second;
    }

    ResourceHandle RenderGraph::DeclareTexture(const std::string& name, const GraphTextureDesc& desc)
    {
        const auto existing = m_NameToHandle.find(name);
        if (existing != m_NameToHandle.end())
            return existing->second;

        const ResourceHandle handle = static_cast<ResourceHandle>(m_NameToHandle.size());
        m_NameToHandle[name] = handle;
        m_Pool->RegisterTexture(handle, desc);

        if (handle >= m_CurrentLayouts.size())
            m_CurrentLayouts.resize(handle + 1, RHI::ImageLayout::Undefined);

        return handle;
    }

    ResourceHandle RenderGraph::DeclareWrite(size_t passIndex, const std::string& name, RHI::ImageLayout finalLayoutAfterExecute)
    {
        const ResourceHandle handle = resolveHandle(name);
        m_PassDeps[passIndex].m_Writes.push_back(handle);
        m_PassDeps[passIndex].m_AllHandles.push_back(handle);
        m_WriteFinalLayout[handle] = finalLayoutAfterExecute;
        return handle;
    }

    ResourceHandle RenderGraph::DeclareRead(size_t passIndex, const std::string& name)
    {
        const ResourceHandle handle = resolveHandle(name);
        m_PassDeps[passIndex].m_AllHandles.push_back(handle);
        return handle;
    }

    ResourceHandle RenderGraph::DeclareReadSampled(size_t passIndex, const std::string& name)
    {
        const ResourceHandle handle = resolveHandle(name);
        m_PassDeps[passIndex].m_ReadSampled.push_back(handle);
        m_PassDeps[passIndex].m_AllHandles.push_back(handle);
        return handle;
    }
}

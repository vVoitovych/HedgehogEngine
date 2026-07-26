#include "RenderGraph.hpp"

#include "IRenderPass.hpp"
#include "RenderGraphBuilder.hpp"
#include "RenderGraphResourcePool.hpp"
#include "RenderResourceLedger.hpp"

#include "Profiling/FrameStats.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHICommandList.hpp"

#include <algorithm>
#include <cassert>

namespace Renderer
{
    RenderGraph::RenderGraph(RenderResourceLedger& ledger)
        : m_Ledger(ledger)
    {
        m_Pool = std::make_unique<RenderGraphResourcePool>();
    }

    RenderGraph::~RenderGraph()
    {
    }

    void RenderGraph::SetScope(std::string scope)
    {
        m_Scope = std::move(scope);
    }

    void RenderGraph::SetStatsSuffix(std::string suffix)
    {
        m_StatsSuffix = std::move(suffix);
    }

    std::string RenderGraph::scopedNameOf(const std::string& localName) const
    {
        return m_Scope.empty() ? localName : (m_Scope + "/" + localName);
    }

    void RenderGraph::AddPass(IRenderPass* pass)
    {
        assert(pass && "Cannot add a null pass to the render graph.");
        m_PassIndexByPointer[pass] = m_Passes.size();
        m_Passes.push_back(pass);

        PassDeps deps;
        deps.ZoneName = std::string(pass->GetName()) + m_StatsSuffix;
        m_PassDeps.push_back(std::move(deps));
    }

    size_t RenderGraph::AddAndCompilePass(IRenderPass* pass, RHI::IRHIDevice& device)
    {
        const size_t index = m_Passes.size();
        AddPass(pass);

        RenderGraphBuilder builder(*this, index);
        pass->Setup(builder);
        pass->CreateFramebuffers(device, *this);

        return index;
    }

    std::vector<ResourceHandle> RenderGraph::publishRecreated(const std::vector<uint32_t>& changedPoolIndices)
    {
        std::vector<ResourceHandle> changedHandles;

        for (size_t h = 0; h < m_Handles.size(); ++h)
        {
            const HandleInfo& info = m_Handles[h];
            if (info.IsImported)
                continue;
            if (std::find(changedPoolIndices.begin(), changedPoolIndices.end(), info.LocalPoolIndex) == changedPoolIndices.end())
                continue;

            m_Ledger.SetLayout(info.ScopedName, RHI::ImageLayout::Undefined);
            m_Ledger.Publish(info.ScopedName, &m_Pool->GetTexture(info.LocalPoolIndex));
            changedHandles.push_back(static_cast<ResourceHandle>(h));
        }

        return changedHandles;
    }

    void RenderGraph::Compile(RHI::IRHIDevice& device,
                              uint32_t swapchainWidth, uint32_t swapchainHeight,
                              uint32_t viewWidth, uint32_t viewHeight)
    {
        m_Pool->SetSwapchainSize(swapchainWidth, swapchainHeight);
        m_Pool->SetViewSize(viewWidth, viewHeight);

        for (size_t i = 0; i < m_Passes.size(); ++i)
        {
            RenderGraphBuilder builder(*this, i);
            m_Passes[i]->Setup(builder);
        }

        for (const SizeClass sizeClass : { SizeClass::SwapchainRelative, SizeClass::ViewRelative, SizeClass::Fixed })
            publishRecreated(m_Pool->Recreate(sizeClass, device));

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
        for (const ResourceHandle handle : m_PassDeps[passIndex].ReadSampled)
        {
            const std::string& scopedName = m_Handles[handle].ScopedName;
            const RHI::ImageLayout oldLayout = m_Ledger.GetLayout(scopedName);
            if (oldLayout == RHI::ImageLayout::ShaderReadOnly)
                continue;

            cmd.TransitionTexture(GetTexture(handle), oldLayout, RHI::ImageLayout::ShaderReadOnly);
            m_Ledger.SetLayout(scopedName, RHI::ImageLayout::ShaderReadOnly);
        }
    }

    void RenderGraph::executePassAt(size_t passIndex, RenderGraphContext& ctx, FrameStats& stats)
    {
        assert(ctx.CommandList && "RenderGraphContext::CommandList must be set before Execute().");

        IRenderPass* pass = m_Passes[passIndex];

        // Tracy's ZoneScopedN (behind HH_PROFILE_ZONE) needs a compile-time string literal, so
        // it can't take the cached ZoneName here — each pass's own Execute() opens its Tracy zone
        // with a literal instead (same base name, no suffix; Tracy merges multiple views' zones
        // under one name — acceptable, see workflow/current-plan.md). ScopedCpuSample has no such
        // constraint: it does a name-keyed lookup into FrameStats at runtime.
        ScopedCpuSample sample(stats, m_PassDeps[passIndex].ZoneName.c_str());

        transitionSampledReads(passIndex, *ctx.CommandList);

        pass->Execute(ctx);

        for (const ResourceHandle handle : m_PassDeps[passIndex].Writes)
            m_Ledger.SetLayout(m_Handles[handle].ScopedName, m_WriteFinalLayout.at(handle));
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

    void RenderGraph::ExecutePassAt(size_t passIndex, RenderGraphContext& ctx, FrameStats& stats)
    {
        assert(passIndex < m_Passes.size() && "ExecutePassAt() called with an index this graph never returned.");
        executePassAt(passIndex, ctx, stats);
    }

    void RenderGraph::SetSwapchainSize(uint32_t width, uint32_t height)
    {
        m_Pool->SetSwapchainSize(width, height);
    }

    void RenderGraph::SetViewSize(uint32_t width, uint32_t height)
    {
        m_Pool->SetViewSize(width, height);
    }

    void RenderGraph::SetFixedSize(const std::string& name, uint32_t width, uint32_t height)
    {
        const ResourceHandle handle = resolveHandle(name);
        assert(!m_Handles[handle].IsImported && "Cannot set the fixed size of an imported resource; set it on the owning graph.");
        m_Pool->SetFixedSize(m_Handles[handle].LocalPoolIndex, width, height);
    }

    void RenderGraph::createFramebuffersForHandles(const std::vector<ResourceHandle>& handles, RHI::IRHIDevice& device)
    {
        for (size_t i = 0; i < m_Passes.size(); ++i)
        {
            const bool dependsOnChange = std::any_of(
                m_PassDeps[i].AllHandles.begin(), m_PassDeps[i].AllHandles.end(),
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
        const std::vector<ResourceHandle> changed = publishRecreated(m_Pool->Recreate(sizeClass, device));
        createFramebuffersForHandles(changed, device);
    }

    void RenderGraph::RefreshImports(RHI::IRHIDevice& device)
    {
        std::vector<ResourceHandle> changed;

        for (size_t h = 0; h < m_Handles.size(); ++h)
        {
            HandleInfo& info = m_Handles[h];
            if (!info.IsImported)
                continue;

            const uint32_t currentGeneration = m_Ledger.GetGeneration(info.ScopedName);
            if (currentGeneration == info.LastSeenGeneration)
                continue;

            info.LastSeenGeneration = currentGeneration;
            changed.push_back(static_cast<ResourceHandle>(h));
        }

        if (!changed.empty())
            createFramebuffersForHandles(changed, device);
    }

    RHI::IRHITexture& RenderGraph::GetTexture(ResourceHandle handle) const
    {
        assert(handle < m_Handles.size() && "Unknown graph resource handle.");
        const HandleInfo& info = m_Handles[handle];
        if (info.IsImported)
        {
            RHI::IRHITexture* texture = m_Ledger.Resolve(info.ScopedName);
            assert(texture && "Imported graph resource not yet published by its owning graph — check graph compile order.");
            return *texture;
        }
        return m_Pool->GetTexture(info.LocalPoolIndex);
    }

    RHI::IRHITexture& RenderGraph::GetTexture(const std::string& name) const
    {
        return GetTexture(resolveAnyHandle(name));
    }

    void RenderGraph::Cleanup(RHI::IRHIDevice& device)
    {
        (void)device;
        m_Pool->Cleanup();
        if (!m_Scope.empty())
            m_Ledger.Erase(m_Scope);
    }

    ResourceHandle RenderGraph::resolveHandle(const std::string& name) const
    {
        const auto it = m_NameToHandle.find(name);
        assert(it != m_NameToHandle.end() && "Reading/writing an undeclared graph resource — the owning pass must run its Setup() first.");
        return it->second;
    }

    ResourceHandle RenderGraph::resolveAnyHandle(const std::string& name) const
    {
        const auto localIt = m_NameToHandle.find(name);
        if (localIt != m_NameToHandle.end())
            return localIt->second;

        const auto importIt = m_ImportNameToHandle.find(name);
        assert(importIt != m_ImportNameToHandle.end() &&
               "GetTexture() on a name that was neither locally declared nor imported — the owning pass must run its Setup() first.");
        return importIt->second;
    }

    ResourceHandle RenderGraph::registerImportHandle(const std::string& scopedName)
    {
        const auto existing = m_ImportNameToHandle.find(scopedName);
        if (existing != m_ImportNameToHandle.end())
            return existing->second;

        HandleInfo info;
        info.ScopedName         = scopedName;
        info.IsImported         = true;
        info.LastSeenGeneration = m_Ledger.GetGeneration(scopedName);

        const ResourceHandle handle = static_cast<ResourceHandle>(m_Handles.size());
        m_Handles.push_back(std::move(info));
        m_ImportNameToHandle[scopedName] = handle;
        return handle;
    }

    ResourceHandle RenderGraph::DeclareTexture(const std::string& name, const GraphTextureDesc& desc)
    {
        const auto existing = m_NameToHandle.find(name);
        if (existing != m_NameToHandle.end())
            return existing->second;

        HandleInfo info;
        info.ScopedName     = scopedNameOf(name);
        info.IsImported     = false;
        info.LocalPoolIndex = m_Pool->RegisterTexture(desc);

        const ResourceHandle handle = static_cast<ResourceHandle>(m_Handles.size());
        m_Handles.push_back(std::move(info));
        m_NameToHandle[name] = handle;
        return handle;
    }

    ResourceHandle RenderGraph::ImportTexture(const std::string& scopedName)
    {
        return registerImportHandle(scopedName);
    }

    void RenderGraph::recordRead(size_t passIndex, ResourceHandle handle)
    {
        m_PassDeps[passIndex].AllHandles.push_back(handle);
    }

    void RenderGraph::recordReadSampled(size_t passIndex, ResourceHandle handle)
    {
        m_PassDeps[passIndex].ReadSampled.push_back(handle);
        m_PassDeps[passIndex].AllHandles.push_back(handle);
    }

    void RenderGraph::recordWrite(size_t passIndex, ResourceHandle handle, RHI::ImageLayout finalLayoutAfterExecute)
    {
        m_PassDeps[passIndex].Writes.push_back(handle);
        m_PassDeps[passIndex].AllHandles.push_back(handle);
        m_WriteFinalLayout[handle] = finalLayoutAfterExecute;
    }

    ResourceHandle RenderGraph::DeclareWrite(size_t passIndex, const std::string& name, RHI::ImageLayout finalLayoutAfterExecute)
    {
        const ResourceHandle handle = resolveHandle(name);
        recordWrite(passIndex, handle, finalLayoutAfterExecute);
        return handle;
    }

    ResourceHandle RenderGraph::DeclareRead(size_t passIndex, const std::string& name)
    {
        const ResourceHandle handle = resolveHandle(name);
        recordRead(passIndex, handle);
        return handle;
    }

    ResourceHandle RenderGraph::DeclareReadSampled(size_t passIndex, const std::string& name)
    {
        const ResourceHandle handle = resolveHandle(name);
        recordReadSampled(passIndex, handle);
        return handle;
    }

    ResourceHandle RenderGraph::DeclareImportWrite(size_t passIndex, const std::string& scopedName, RHI::ImageLayout finalLayoutAfterExecute)
    {
        const ResourceHandle handle = registerImportHandle(scopedName);
        recordWrite(passIndex, handle, finalLayoutAfterExecute);
        return handle;
    }

    ResourceHandle RenderGraph::DeclareImportRead(size_t passIndex, const std::string& scopedName)
    {
        const ResourceHandle handle = registerImportHandle(scopedName);
        recordRead(passIndex, handle);
        return handle;
    }

    ResourceHandle RenderGraph::DeclareImportReadSampled(size_t passIndex, const std::string& scopedName)
    {
        const ResourceHandle handle = registerImportHandle(scopedName);
        recordReadSampled(passIndex, handle);
        return handle;
    }
}

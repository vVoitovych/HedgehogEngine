#include "PipelineManager.hpp"

#include "ShaderManager/ShaderManager.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHIShader.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace Renderer
{

namespace
{
    void HashCombine(size_t& seed, size_t value)
    {
        seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    void HashPtr(size_t& seed, const void* ptr)
    {
        HashCombine(seed, reinterpret_cast<uintptr_t>(ptr));
    }

    void HashU32(size_t& seed, uint32_t v)
    {
        HashCombine(seed, static_cast<size_t>(v));
    }

    void HashFloat(size_t& seed, float v)
    {
        uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        HashU32(seed, bits);
    }
} // namespace

PipelineManager::PipelineManager(RHI::IRHIDevice& device, ShaderManager& shaderManager)
    : m_Device(device)
    , m_ShaderManager(shaderManager)
{}

void PipelineManager::Cleanup()
{
    m_Cache.clear();
}

size_t PipelineManager::HashDesc(const RHI::GraphicsPipelineDesc& desc)
{
    size_t seed = 0;

    HashPtr(seed, desc.m_VertexShader);
    HashPtr(seed, desc.m_FragmentShader);
    HashPtr(seed, desc.m_RenderPass);
    HashU32(seed, desc.m_Subpass);
    HashU32(seed, static_cast<uint32_t>(desc.m_Topology));
    HashU32(seed, static_cast<uint32_t>(desc.m_CullMode));
    HashU32(seed, static_cast<uint32_t>(desc.m_FillMode));
    HashFloat(seed, desc.m_LineWidth);
    HashU32(seed, static_cast<uint32_t>(desc.m_DepthTestEnable));
    HashU32(seed, static_cast<uint32_t>(desc.m_DepthWriteEnable));
    HashU32(seed, static_cast<uint32_t>(desc.m_DepthCompareOp));
    HashU32(seed, static_cast<uint32_t>(desc.m_StencilTestEnable));

    for (const auto* layout : desc.m_DescriptorSetLayouts)
        HashPtr(seed, layout);

    for (const auto& b : desc.m_VertexBindings)
    {
        HashU32(seed, b.m_Binding);
        HashU32(seed, b.m_Stride);
        HashU32(seed, static_cast<uint32_t>(b.m_InputRate));
    }

    for (const auto& a : desc.m_VertexAttributes)
    {
        HashU32(seed, a.m_Location);
        HashU32(seed, a.m_Binding);
        HashU32(seed, static_cast<uint32_t>(a.m_Format));
        HashU32(seed, a.m_Offset);
    }

    for (const auto& att : desc.m_ColorBlendAttachments)
    {
        HashU32(seed, static_cast<uint32_t>(att.m_BlendEnable));
        HashU32(seed, static_cast<uint32_t>(att.m_SrcColorFactor));
        HashU32(seed, static_cast<uint32_t>(att.m_DstColorFactor));
        HashU32(seed, static_cast<uint32_t>(att.m_ColorOp));
        HashU32(seed, static_cast<uint32_t>(att.m_SrcAlphaFactor));
        HashU32(seed, static_cast<uint32_t>(att.m_DstAlphaFactor));
        HashU32(seed, static_cast<uint32_t>(att.m_AlphaOp));
    }

    for (const auto& pc : desc.m_PushConstantRanges)
    {
        HashU32(seed, static_cast<uint32_t>(pc.m_Stages));
        HashU32(seed, pc.m_Offset);
        HashU32(seed, pc.m_Size);
    }

    return seed;
}

RHI::IRHIPipeline& PipelineManager::GetOrCreate(const RHI::GraphicsPipelineDesc& desc)
{
    const size_t key = HashDesc(desc);
    auto it = m_Cache.find(key);
    if (it != m_Cache.end())
    {
        LOGINFO("PipelineManager: cache hit");
        return *it->second.pipeline;
    }

    LOGINFO("PipelineManager: creating new pipeline");
    auto pipeline = m_Device.CreateGraphicsPipeline(desc);
    assert(pipeline && "PipelineManager: failed to create pipeline");

    RHI::IRHIPipeline* ptr = pipeline.get();
    m_Cache[key] = { desc, std::move(pipeline) };
    return *ptr;
}

void PipelineManager::InvalidateShadersOf(const std::string& spirvPath)
{
    const RHI::IRHIShader* vert = m_ShaderManager.TryGet(spirvPath, RHI::ShaderStage::Vertex);
    const RHI::IRHIShader* frag = m_ShaderManager.TryGet(spirvPath, RHI::ShaderStage::Fragment);

    if (!vert && !frag)
        return;

    for (auto it = m_Cache.begin(); it != m_Cache.end(); )
    {
        const auto& d = it->second.desc;
        if ((vert && d.m_VertexShader   == vert) ||
            (frag && d.m_FragmentShader == frag))
        {
            LOGINFO("PipelineManager: invalidating pipeline for '", spirvPath, "'");
            it = m_Cache.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

std::vector<RHI::PoolSize> PipelineManager::MakePoolSizes(
    const std::vector<RHI::DescriptorBinding>& bindings,
    uint32_t maxSets)
{
    std::vector<RHI::PoolSize> sizes;
    for (const auto& b : bindings)
    {
        auto it = std::find_if(sizes.begin(), sizes.end(),
            [&](const RHI::PoolSize& ps) { return ps.m_Type == b.m_Type; });
        if (it != sizes.end())
            it->m_Count += b.m_Count * maxSets;
        else
            sizes.push_back({ b.m_Type, b.m_Count * maxSets });
    }
    return sizes;
}

} // namespace Renderer

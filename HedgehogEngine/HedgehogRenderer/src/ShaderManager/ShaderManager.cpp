#include "ShaderManager.hpp"

#include "Logger/api/Logger.hpp"

#include "RHI/api/IRHIDevice.hpp"

#include <yaml-cpp/yaml.h>

#include <Windows.h>
#include <cassert>
#include <filesystem>
#include <sstream>

namespace Renderer
{

namespace
{
    std::filesystem::path GetRepoRoot()
    {
        char buffer[MAX_PATH];
        GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        return std::filesystem::path(buffer)
            .parent_path().parent_path().parent_path().parent_path().parent_path();
    }

    std::string ToRepoRelative(const std::filesystem::path& baseDir, const std::string& relPath)
    {
        const auto abs = (baseDir / relPath).lexically_normal();
        const auto rel = abs.lexically_relative(GetRepoRoot());
        return "/" + rel.generic_string();
    }

    std::string ResolveAbsPath(const std::string& repoRelativePath)
    {
        return (GetRepoRoot() / repoRelativePath.substr(1)).lexically_normal().string();
    }
} // namespace

// ── Construction / cleanup ────────────────────────────────────────────────────

ShaderManager::ShaderManager(RHI::IRHIDevice& device)
    : m_Device(device)
{}

void ShaderManager::Cleanup()
{
    m_Cache.clear();
}

// ── Cache key ─────────────────────────────────────────────────────────────────

std::string ShaderManager::MakeCacheKey(const std::string& path, RHI::ShaderStage stage)
{
    return path + "|" + std::to_string(static_cast<uint32_t>(stage));
}

// ── Shader cache ─────────────────────────────────────────────────────────────

RHI::IRHIShader& ShaderManager::GetOrLoad(const std::string& spirvPath, RHI::ShaderStage stage)
{
    const std::string key = MakeCacheKey(spirvPath, stage);
    auto it = m_Cache.find(key);
    if (it != m_Cache.end())
    {
        LOGINFO("ShaderManager: cache hit '", spirvPath, "'");
        return *it->second;
    }

    LOGINFO("ShaderManager: loading '", spirvPath, "'");
    auto shader = m_Device.CreateShader(spirvPath, stage);
    assert(shader && "ShaderManager: failed to create shader");
    RHI::IRHIShader* ptr = shader.get();
    m_Cache[key] = std::move(shader);
    return *ptr;
}

const RHI::IRHIShader* ShaderManager::TryGet(const std::string& spirvPath,
                                              RHI::ShaderStage   stage) const
{
    auto it = m_Cache.find(MakeCacheKey(spirvPath, stage));
    return it != m_Cache.end() ? it->second.get() : nullptr;
}

// ── .pl pipeline layout parser ────────────────────────────────────────────────

RHI::DescriptorType ShaderManager::ParseDescriptorType(const std::string& s)
{
    if (s == "uniform_buffer")         return RHI::DescriptorType::UniformBuffer;
    if (s == "storage_buffer")         return RHI::DescriptorType::StorageBuffer;
    if (s == "combined_image_sampler") return RHI::DescriptorType::CombinedImageSampler;
    if (s == "storage_image")          return RHI::DescriptorType::StorageImage;
    if (s == "input_attachment")       return RHI::DescriptorType::InputAttachment;
    LOGERROR("ShaderManager: unknown descriptor type '", s, "'");
    assert(false && "Unknown descriptor type in .pl file");
    return RHI::DescriptorType::UniformBuffer;
}

RHI::ShaderStage ShaderManager::ParseDescriptorStage(const std::string& s)
{
    RHI::ShaderStage result = RHI::ShaderStage::None;
    std::string token;
    std::istringstream ss(s);
    while (std::getline(ss, token, '|'))
    {
        while (!token.empty() && token.front() == ' ') token.erase(token.begin());
        while (!token.empty() && token.back()  == ' ') token.pop_back();

        if      (token == "vertex")   result = result | RHI::ShaderStage::Vertex;
        else if (token == "fragment") result = result | RHI::ShaderStage::Fragment;
        else if (token == "compute")  result = result | RHI::ShaderStage::Compute;
        else if (token == "all")      result = RHI::ShaderStage::All;
        else
        {
            LOGERROR("ShaderManager: unknown shader stage '", token, "'");
            assert(false && "Unknown shader stage in .pl file");
        }
    }
    return result;
}

PipelineFileDesc ShaderManager::LoadPipelineLayout(const std::string& layoutPath)
{
    const std::string fullPath = ResolveAbsPath(layoutPath);

    YAML::Node root;
    try { root = YAML::LoadFile(fullPath); }
    catch (const YAML::Exception& e)
    {
        LOGERROR("ShaderManager: failed to load layout '", fullPath, "': ", e.what());
        assert(false && "Pipeline layout file not found or malformed.");
    }

    PipelineFileDesc desc;

    if (const YAML::Node& sets = root["descriptor_sets"])
    {
        for (const YAML::Node& setNode : sets)
        {
            std::vector<RHI::DescriptorBinding> bindings;
            if (const YAML::Node& bindingsNode = setNode["bindings"])
            {
                for (const YAML::Node& b : bindingsNode)
                {
                    RHI::DescriptorBinding binding;
                    binding.m_Binding = b["binding"].as<uint32_t>();
                    binding.m_Type    = ParseDescriptorType(b["type"].as<std::string>());
                    binding.m_Count   = b["count"] ? b["count"].as<uint32_t>() : 1u;
                    binding.m_Stages  = ParseDescriptorStage(b["stage"].as<std::string>());
                    bindings.push_back(binding);
                }
            }
            desc.m_DescriptorSets.push_back(std::move(bindings));
        }
    }

    if (const YAML::Node& pcs = root["push_constants"])
    {
        for (const YAML::Node& pc : pcs)
        {
            RHI::PushConstantRange range;
            range.m_Stages = ParseDescriptorStage(pc["stage"].as<std::string>());
            range.m_Offset = pc["offset"] ? pc["offset"].as<uint32_t>() : 0u;
            range.m_Size   = pc["size"].as<uint32_t>();
            desc.m_PushConstants.push_back(range);
        }
    }

    return desc;
}

// ── .vdes vertex description parser ──────────────────────────────────────────

RHI::Format ShaderManager::ParseVertexFormat(const std::string& s)
{
    if (s == "r32_float")          return RHI::Format::R32Float;
    if (s == "r32g32_float")       return RHI::Format::R32G32Float;
    if (s == "r32g32b32_float")    return RHI::Format::R32G32B32Float;
    if (s == "r32g32b32a32_float") return RHI::Format::R32G32B32A32Float;
    LOGERROR("ShaderManager: unknown vertex format '", s, "'");
    assert(false && "Unknown format in .vdes file");
    return RHI::Format::Undefined;
}

RHI::VertexInputRate ShaderManager::ParseVertexInputRate(const std::string& s)
{
    if (s == "per_vertex")   return RHI::VertexInputRate::PerVertex;
    if (s == "per_instance") return RHI::VertexInputRate::PerInstance;
    LOGERROR("ShaderManager: unknown input rate '", s, "'");
    assert(false && "Unknown input rate in .vdes file");
    return RHI::VertexInputRate::PerVertex;
}

// ── .shader file loader ────────────────────────────────────────────────────────

ShaderPipelineDesc ShaderManager::LoadShaderFile(const std::string& shaderPath)
{
    const auto repoRoot  = GetRepoRoot();
    const auto absShader = (repoRoot / shaderPath.substr(1)).lexically_normal();
    const auto shaderDir = absShader.parent_path();

    YAML::Node root;
    try { root = YAML::LoadFile(absShader.string()); }
    catch (const YAML::Exception& e)
    {
        LOGERROR("ShaderManager: failed to load '", absShader.string(), "': ", e.what());
        assert(false && "Shader file not found or malformed.");
    }

    ShaderPipelineDesc desc;

    // Pipeline layout (required)
    if (const YAML::Node& n = root["pipeline_layout"])
    {
        desc.m_Layout                        = LoadPipelineLayout(ToRepoRelative(shaderDir, n.as<std::string>()));
        desc.m_Pipeline.m_PushConstantRanges = desc.m_Layout.m_PushConstants;
    }
    else
    {
        LOGERROR("ShaderManager: 'pipeline_layout' missing in '", absShader.string(), "'");
        assert(false && "Missing pipeline_layout in .shader file");
    }

    // Vertex description (optional)
    if (const YAML::Node& n = root["vertex_description"])
    {
        const std::string vdesPath = ToRepoRelative(shaderDir, n.as<std::string>());
        const std::string fullPath = ResolveAbsPath(vdesPath);

        YAML::Node vdes;
        try { vdes = YAML::LoadFile(fullPath); }
        catch (const YAML::Exception& e)
        {
            LOGERROR("ShaderManager: failed to load vertex desc '", fullPath, "': ", e.what());
            assert(false && "Vertex description file not found or malformed.");
        }

        if (const YAML::Node& bindingsNode = vdes["bindings"])
        {
            for (const YAML::Node& b : bindingsNode)
            {
                RHI::VertexBinding binding;
                binding.m_Binding   = b["binding"].as<uint32_t>();
                binding.m_Stride    = b["stride"].as<uint32_t>();
                binding.m_InputRate = b["input_rate"]
                    ? ParseVertexInputRate(b["input_rate"].as<std::string>())
                    : RHI::VertexInputRate::PerVertex;
                desc.m_Pipeline.m_VertexBindings.push_back(binding);
            }
        }

        if (const YAML::Node& attrsNode = vdes["attributes"])
        {
            for (const YAML::Node& a : attrsNode)
            {
                RHI::VertexAttribute attr;
                attr.m_Location = a["location"].as<uint32_t>();
                attr.m_Binding  = a["binding"].as<uint32_t>();
                attr.m_Format   = ParseVertexFormat(a["format"].as<std::string>());
                attr.m_Offset   = a["offset"] ? a["offset"].as<uint32_t>() : 0u;
                desc.m_Pipeline.m_VertexAttributes.push_back(attr);
            }
        }
    }

    // Topology (optional)
    if (const YAML::Node& n = root["topology"])
        desc.m_Pipeline.m_Topology = ParseTopology(n.as<std::string>());

    // Rasterization (optional)
    if (const YAML::Node& n = root["rasterization"])
    {
        if (const YAML::Node& cm = n["cull_mode"]) desc.m_Pipeline.m_CullMode = ParseCullMode(cm.as<std::string>());
        if (const YAML::Node& fm = n["fill_mode"]) desc.m_Pipeline.m_FillMode = ParseFillMode(fm.as<std::string>());
    }

    // Depth (optional)
    if (const YAML::Node& n = root["depth"])
    {
        if (const YAML::Node& t = n["test"])    desc.m_Pipeline.m_DepthTestEnable  = t.as<bool>();
        if (const YAML::Node& w = n["write"])   desc.m_Pipeline.m_DepthWriteEnable = w.as<bool>();
        if (const YAML::Node& c = n["compare"]) desc.m_Pipeline.m_DepthCompareOp   = ParseCompareOp(c.as<std::string>());
    }

    // Blend attachments (optional)
    if (const YAML::Node& blends = root["blend"])
    {
        for (const YAML::Node& b : blends)
        {
            RHI::ColorBlendAttachment att;
            if (const YAML::Node& e = b["enabled"])   att.m_BlendEnable    = e.as<bool>();
            if (const YAML::Node& f = b["src_color"]) att.m_SrcColorFactor = ParseBlendFactor(f.as<std::string>());
            if (const YAML::Node& f = b["dst_color"]) att.m_DstColorFactor = ParseBlendFactor(f.as<std::string>());
            if (const YAML::Node& o = b["color_op"])  att.m_ColorOp        = ParseBlendOp(o.as<std::string>());
            if (const YAML::Node& f = b["src_alpha"]) att.m_SrcAlphaFactor = ParseBlendFactor(f.as<std::string>());
            if (const YAML::Node& f = b["dst_alpha"]) att.m_DstAlphaFactor = ParseBlendFactor(f.as<std::string>());
            if (const YAML::Node& o = b["alpha_op"])  att.m_AlphaOp        = ParseBlendOp(o.as<std::string>());
            desc.m_Pipeline.m_ColorBlendAttachments.push_back(att);
        }
    }

    // Shader stages
    if (const YAML::Node& stages = root["shaders"])
    {
        for (const YAML::Node& s : stages)
        {
            const RHI::ShaderStage stage   = ParseStage(s["stage"].as<std::string>());
            const std::string      spvPath = ToRepoRelative(shaderDir, s["path"].as<std::string>());

            RHI::IRHIShader& shader = GetOrLoad(spvPath, stage);

            switch (stage)
            {
            case RHI::ShaderStage::Vertex:
                desc.m_VertexShader            = &shader;
                desc.m_Pipeline.m_VertexShader = &shader;
                break;
            case RHI::ShaderStage::Fragment:
                desc.m_FragmentShader            = &shader;
                desc.m_Pipeline.m_FragmentShader = &shader;
                break;
            default:
                LOGERROR("ShaderManager: unhandled stage '", s["stage"].as<std::string>(), "'");
                assert(false && "Unhandled shader stage in .shader file");
                break;
            }
        }
    }

    assert(desc.m_VertexShader && "A .shader file must contain at least a vertex stage");
    return desc;
}

// ── Parse helpers ────────────────────────────────────────────────────────────

RHI::ShaderStage ShaderManager::ParseStage(const std::string& s)
{
    if (s == "vertex")   return RHI::ShaderStage::Vertex;
    if (s == "fragment") return RHI::ShaderStage::Fragment;
    if (s == "compute")  return RHI::ShaderStage::Compute;
    LOGERROR("ShaderManager: unknown stage '", s, "'");
    assert(false && "Unknown shader stage in .shader file");
    return RHI::ShaderStage::Vertex;
}

RHI::PrimitiveTopology ShaderManager::ParseTopology(const std::string& s)
{
    if (s == "triangle_list")  return RHI::PrimitiveTopology::TriangleList;
    if (s == "triangle_strip") return RHI::PrimitiveTopology::TriangleStrip;
    if (s == "line_list")      return RHI::PrimitiveTopology::LineList;
    if (s == "point_list")     return RHI::PrimitiveTopology::PointList;
    LOGERROR("ShaderManager: unknown topology '", s, "'");
    assert(false && "Unknown topology in .shader file");
    return RHI::PrimitiveTopology::TriangleList;
}

RHI::CullMode ShaderManager::ParseCullMode(const std::string& s)
{
    if (s == "none")  return RHI::CullMode::None;
    if (s == "front") return RHI::CullMode::Front;
    if (s == "back")  return RHI::CullMode::Back;
    LOGERROR("ShaderManager: unknown cull_mode '", s, "'");
    assert(false && "Unknown cull_mode in .shader file");
    return RHI::CullMode::Back;
}

RHI::FillMode ShaderManager::ParseFillMode(const std::string& s)
{
    if (s == "solid")     return RHI::FillMode::Solid;
    if (s == "wireframe") return RHI::FillMode::Wireframe;
    LOGERROR("ShaderManager: unknown fill_mode '", s, "'");
    assert(false && "Unknown fill_mode in .shader file");
    return RHI::FillMode::Solid;
}

RHI::CompareOp ShaderManager::ParseCompareOp(const std::string& s)
{
    if (s == "never")            return RHI::CompareOp::Never;
    if (s == "less")             return RHI::CompareOp::Less;
    if (s == "equal")            return RHI::CompareOp::Equal;
    if (s == "less_or_equal")    return RHI::CompareOp::LessOrEqual;
    if (s == "greater")          return RHI::CompareOp::Greater;
    if (s == "not_equal")        return RHI::CompareOp::NotEqual;
    if (s == "greater_or_equal") return RHI::CompareOp::GreaterOrEqual;
    if (s == "always")           return RHI::CompareOp::Always;
    LOGERROR("ShaderManager: unknown compare op '", s, "'");
    assert(false && "Unknown compare op in .shader file");
    return RHI::CompareOp::Less;
}

RHI::BlendFactor ShaderManager::ParseBlendFactor(const std::string& s)
{
    if (s == "zero")                return RHI::BlendFactor::Zero;
    if (s == "one")                 return RHI::BlendFactor::One;
    if (s == "src_alpha")           return RHI::BlendFactor::SrcAlpha;
    if (s == "one_minus_src_alpha") return RHI::BlendFactor::OneMinusSrcAlpha;
    if (s == "dst_alpha")           return RHI::BlendFactor::DstAlpha;
    if (s == "one_minus_dst_alpha") return RHI::BlendFactor::OneMinusDstAlpha;
    if (s == "src_color")           return RHI::BlendFactor::SrcColor;
    if (s == "one_minus_src_color") return RHI::BlendFactor::OneMinusSrcColor;
    LOGERROR("ShaderManager: unknown blend factor '", s, "'");
    assert(false && "Unknown blend factor in .shader file");
    return RHI::BlendFactor::One;
}

RHI::BlendOp ShaderManager::ParseBlendOp(const std::string& s)
{
    if (s == "add")              return RHI::BlendOp::Add;
    if (s == "subtract")         return RHI::BlendOp::Subtract;
    if (s == "reverse_subtract") return RHI::BlendOp::ReverseSubtract;
    if (s == "min")              return RHI::BlendOp::Min;
    if (s == "max")              return RHI::BlendOp::Max;
    LOGERROR("ShaderManager: unknown blend op '", s, "'");
    assert(false && "Unknown blend op in .shader file");
    return RHI::BlendOp::Add;
}

} // namespace Renderer

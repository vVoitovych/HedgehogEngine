#include "ShaderManager.hpp"
#include "Pipeline/VertexDescLoader.hpp"

#include "Logger/api/Logger.hpp"

#include "RHI/api/IRHIDevice.hpp"

#include <yaml-cpp/yaml.h>

#include <Windows.h>
#include <cassert>
#include <filesystem>

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

    std::string MakeCacheKey(const std::string& path, RHI::ShaderStage stage)
    {
        return path + "|" + std::to_string(static_cast<uint32_t>(stage));
    }
} // namespace

ShaderManager::ShaderManager(RHI::IRHIDevice& device)
    : m_Device(device)
{}

void ShaderManager::Cleanup()
{
    m_Cache.clear();
}

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

ShaderPipelineDesc ShaderManager::LoadShaderFile(const std::string& shaderPath)
{
    const auto repoRoot   = GetRepoRoot();
    const auto absShader  = (repoRoot / shaderPath.substr(1)).lexically_normal();
    const auto shaderDir  = absShader.parent_path();

    YAML::Node root;
    try
    {
        root = YAML::LoadFile(absShader.string());
    }
    catch (const YAML::Exception& e)
    {
        LOGERROR("ShaderManager: failed to load '", absShader.string(), "': ", e.what());
        assert(false && "Shader file not found or malformed.");
    }

    ShaderPipelineDesc desc;

    // Pipeline layout (required) — also seeds push constant ranges
    if (const YAML::Node& n = root["pipeline_layout"])
    {
        desc.m_Layout                        = PipelineLoader::Load(ToRepoRelative(shaderDir, n.as<std::string>()));
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
        const auto vd                      = VertexDescLoader::Load(ToRepoRelative(shaderDir, n.as<std::string>()));
        desc.m_Pipeline.m_VertexBindings   = vd.m_Bindings;
        desc.m_Pipeline.m_VertexAttributes = vd.m_Attributes;
    }

    // Topology (optional, default: triangle_list)
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

    // Color blend attachments (optional, default: empty = depth-only pass)
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

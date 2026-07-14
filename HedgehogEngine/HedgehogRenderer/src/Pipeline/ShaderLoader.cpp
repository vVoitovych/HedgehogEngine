#include "ShaderLoader.hpp"
#include "VertexDescLoader.hpp"

#include "Logger/api/Logger.hpp"

#include "RHI/api/IRHIDevice.hpp"

#include <yaml-cpp/yaml.h>

#include <cassert>
#include <filesystem>

namespace Renderer
{

namespace
{
    // Resolve a path relative to a virtual directory.
    // virtualDir  — the directory portion of the parent virtual path, e.g. "engine://a/b/"
    // relPath     — a relative path from within the file, e.g. "../Pipelines/Foo.pl"
    // Returns a normalised virtual path, e.g. "engine://a/Pipelines/Foo.pl".
    std::string ResolveVirtualRelative(const std::string& virtualDir,
                                       const std::string& relPath)
    {
        // Split off the alias prefix (everything up to and including "://").
        const auto sepPos = virtualDir.find("://");
        const std::string alias   = virtualDir.substr(0, sepPos + 3);  // e.g. "engine://"
        const std::string dirPart = virtualDir.substr(sepPos + 3);     // e.g. "a/b/"

        // Use std::filesystem path arithmetic then convert back to generic (forward-slash) form.
        const std::filesystem::path resolved =
            (std::filesystem::path(dirPart) / relPath).lexically_normal();
        return alias + resolved.generic_string();
    }
}

RHI::ShaderStage ShaderLoader::ParseStage(const std::string& s)
{
    if (s == "vertex")   return RHI::ShaderStage::Vertex;
    if (s == "fragment") return RHI::ShaderStage::Fragment;
    if (s == "compute")  return RHI::ShaderStage::Compute;
    LOGERROR("ShaderLoader: unknown stage '", s, "'");
    assert(false && "Unknown shader stage in .shader file");
    return RHI::ShaderStage::Vertex;
}

RHI::PrimitiveTopology ShaderLoader::ParseTopology(const std::string& s)
{
    if (s == "triangle_list")  return RHI::PrimitiveTopology::TriangleList;
    if (s == "triangle_strip") return RHI::PrimitiveTopology::TriangleStrip;
    if (s == "line_list")      return RHI::PrimitiveTopology::LineList;
    if (s == "point_list")     return RHI::PrimitiveTopology::PointList;
    LOGERROR("ShaderLoader: unknown topology '", s, "'");
    assert(false && "Unknown topology in .shader file");
    return RHI::PrimitiveTopology::TriangleList;
}

RHI::CullMode ShaderLoader::ParseCullMode(const std::string& s)
{
    if (s == "none")  return RHI::CullMode::None;
    if (s == "front") return RHI::CullMode::Front;
    if (s == "back")  return RHI::CullMode::Back;
    LOGERROR("ShaderLoader: unknown cull_mode '", s, "'");
    assert(false && "Unknown cull_mode in .shader file");
    return RHI::CullMode::Back;
}

RHI::FillMode ShaderLoader::ParseFillMode(const std::string& s)
{
    if (s == "solid")     return RHI::FillMode::Solid;
    if (s == "wireframe") return RHI::FillMode::Wireframe;
    LOGERROR("ShaderLoader: unknown fill_mode '", s, "'");
    assert(false && "Unknown fill_mode in .shader file");
    return RHI::FillMode::Solid;
}

RHI::CompareOp ShaderLoader::ParseCompareOp(const std::string& s)
{
    if (s == "never")            return RHI::CompareOp::Never;
    if (s == "less")             return RHI::CompareOp::Less;
    if (s == "equal")            return RHI::CompareOp::Equal;
    if (s == "less_or_equal")    return RHI::CompareOp::LessOrEqual;
    if (s == "greater")          return RHI::CompareOp::Greater;
    if (s == "not_equal")        return RHI::CompareOp::NotEqual;
    if (s == "greater_or_equal") return RHI::CompareOp::GreaterOrEqual;
    if (s == "always")           return RHI::CompareOp::Always;
    LOGERROR("ShaderLoader: unknown compare op '", s, "'");
    assert(false && "Unknown compare op in .shader file");
    return RHI::CompareOp::Less;
}

RHI::BlendFactor ShaderLoader::ParseBlendFactor(const std::string& s)
{
    if (s == "zero")                return RHI::BlendFactor::Zero;
    if (s == "one")                 return RHI::BlendFactor::One;
    if (s == "src_alpha")           return RHI::BlendFactor::SrcAlpha;
    if (s == "one_minus_src_alpha") return RHI::BlendFactor::OneMinusSrcAlpha;
    if (s == "dst_alpha")           return RHI::BlendFactor::DstAlpha;
    if (s == "one_minus_dst_alpha") return RHI::BlendFactor::OneMinusDstAlpha;
    if (s == "src_color")           return RHI::BlendFactor::SrcColor;
    if (s == "one_minus_src_color") return RHI::BlendFactor::OneMinusSrcColor;
    LOGERROR("ShaderLoader: unknown blend factor '", s, "'");
    assert(false && "Unknown blend factor in .shader file");
    return RHI::BlendFactor::One;
}

RHI::BlendOp ShaderLoader::ParseBlendOp(const std::string& s)
{
    if (s == "add")              return RHI::BlendOp::Add;
    if (s == "subtract")         return RHI::BlendOp::Subtract;
    if (s == "reverse_subtract") return RHI::BlendOp::ReverseSubtract;
    if (s == "min")              return RHI::BlendOp::Min;
    if (s == "max")              return RHI::BlendOp::Max;
    LOGERROR("ShaderLoader: unknown blend op '", s, "'");
    assert(false && "Unknown blend op in .shader file");
    return RHI::BlendOp::Add;
}

ShaderPipelineDesc ShaderLoader::Load(RHI::IRHIDevice& device,
                                       const std::string& shaderVirtualPath,
                                       const FS::FileSystemManager& fileSystem)
{
    // Derive the virtual directory of the shader file (everything up to the last '/').
    const std::string shaderVirtualDir =
        shaderVirtualPath.substr(0, shaderVirtualPath.rfind('/') + 1);

    const auto text = fileSystem.ReadTextFile(shaderVirtualPath);
    if (!text)
    {
        LOGERROR("ShaderLoader: failed to read '", shaderVirtualPath, "'");
        assert(false && "Shader file not found or malformed.");
    }

    YAML::Node root;
    try
    {
        root = YAML::Load(*text);
    }
    catch (const YAML::Exception& e)
    {
        LOGERROR("ShaderLoader: failed to parse '", shaderVirtualPath, "': ", e.what());
        assert(false && "Shader file not found or malformed.");
    }

    ShaderPipelineDesc desc;

    // Pipeline layout (required) — also seeds push constant ranges
    if (const YAML::Node& n = root["pipeline_layout"])
    {
        const std::string layoutPath         = ResolveVirtualRelative(shaderVirtualDir, n.as<std::string>());
        desc.m_Layout                        = PipelineLoader::Load(layoutPath, fileSystem);
        desc.m_Pipeline.m_PushConstantRanges = desc.m_Layout.m_PushConstants;
    }
    else
    {
        LOGERROR("ShaderLoader: 'pipeline_layout' missing in '", shaderVirtualPath, "'");
        assert(false && "Missing pipeline_layout in .shader file");
    }

    // Vertex description (optional)
    if (const YAML::Node& n = root["vertex_description"])
    {
        const std::string vdesPath         = ResolveVirtualRelative(shaderVirtualDir, n.as<std::string>());
        const auto vd                      = VertexDescLoader::Load(vdesPath, fileSystem);
        desc.m_Pipeline.m_VertexBindings   = vd.m_Bindings;
        desc.m_Pipeline.m_VertexAttributes = vd.m_Attributes;
    }

    // Topology (optional, default: triangle_list)
    if (const YAML::Node& n = root["topology"])
        desc.m_Pipeline.m_Topology = ParseTopology(n.as<std::string>());

    // Rasterization (optional, defaults: cull_mode=back, fill_mode=solid)
    if (const YAML::Node& n = root["rasterization"])
    {
        if (const YAML::Node& cm = n["cull_mode"]) desc.m_Pipeline.m_CullMode = ParseCullMode(cm.as<std::string>());
        if (const YAML::Node& fm = n["fill_mode"]) desc.m_Pipeline.m_FillMode = ParseFillMode(fm.as<std::string>());
    }

    // Depth (optional, defaults: test=true, write=true, compare=less)
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
            const std::string      spvPath = ResolveVirtualRelative(shaderVirtualDir, s["path"].as<std::string>());

            const auto spirv = fileSystem.ReadFile(spvPath);
            if (!spirv)
            {
                LOGERROR("ShaderLoader: failed to read SPIR-V '", spvPath, "'");
                assert(false && "SPIR-V file not found");
            }
            auto shader = device.CreateShader(*spirv, stage);
            assert(shader && "Failed to create shader");

            switch (stage)
            {
            case RHI::ShaderStage::Vertex:
                desc.m_VertexShader            = std::move(shader);
                desc.m_Pipeline.m_VertexShader = desc.m_VertexShader.get();
                break;
            case RHI::ShaderStage::Fragment:
                desc.m_FragmentShader            = std::move(shader);
                desc.m_Pipeline.m_FragmentShader = desc.m_FragmentShader.get();
                break;
            default:
                LOGERROR("ShaderLoader: unhandled stage '", s["stage"].as<std::string>(), "'");
                assert(false && "Unhandled shader stage");
                break;
            }
        }
    }

    assert(desc.m_VertexShader && "A .shader file must contain at least a vertex stage");
    return desc;
}

} // namespace Renderer

#include "ShaderLoader.hpp"

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

    // Resolve a path that is relative to baseDir into a repo-root-relative path
    // (starting with '/') suitable for PipelineLoader, VertexDescLoader, and CreateShader.
    std::string ToRepoRelative(const std::filesystem::path& baseDir,
                               const std::string&           relPath)
    {
        const auto abs = (baseDir / relPath).lexically_normal();
        const auto rel = abs.lexically_relative(GetRepoRoot());
        return "/" + rel.generic_string();
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

ShaderPipelineDesc ShaderLoader::Load(RHI::IRHIDevice& device, const std::string& shaderPath)
{
    // shaderPath begins with '/' — strip it to make a relative path from repo root.
    const auto repoRoot  = GetRepoRoot();
    const auto absShader = (repoRoot / shaderPath.substr(1)).lexically_normal();
    const auto shaderDir = absShader.parent_path();

    YAML::Node root;
    try
    {
        root = YAML::LoadFile(absShader.string());
    }
    catch (const YAML::Exception& e)
    {
        LOGERROR("ShaderLoader: failed to load '", absShader.string(), "': ", e.what());
        assert(false && "Shader file not found or malformed.");
    }

    ShaderPipelineDesc desc;

    // Pipeline layout (required)
    if (const YAML::Node& n = root["pipeline_layout"])
    {
        desc.m_Layout = PipelineLoader::Load(ToRepoRelative(shaderDir, n.as<std::string>()));
    }
    else
    {
        LOGERROR("ShaderLoader: 'pipeline_layout' missing in '", absShader.string(), "'");
        assert(false && "Missing pipeline_layout in .shader file");
    }

    // Vertex description (optional — compute / depth-less passes may omit it)
    if (const YAML::Node& n = root["vertex_description"])
        desc.m_VertexDesc = VertexDescLoader::Load(ToRepoRelative(shaderDir, n.as<std::string>()));

    // Shader stages
    if (const YAML::Node& stages = root["shaders"])
    {
        for (const YAML::Node& s : stages)
        {
            const RHI::ShaderStage stage   = ParseStage(s["stage"].as<std::string>());
            const std::string      spvPath = ToRepoRelative(shaderDir, s["path"].as<std::string>());

            auto shader = device.CreateShader(spvPath, stage);
            assert(shader && "Failed to create shader");

            switch (stage)
            {
            case RHI::ShaderStage::Vertex:   desc.m_VertexShader   = std::move(shader); break;
            case RHI::ShaderStage::Fragment:  desc.m_FragmentShader = std::move(shader); break;
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

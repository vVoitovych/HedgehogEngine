#include "VertexDescLoader.hpp"

#include "Logger/api/Logger.hpp"

#include <yaml-cpp/yaml.h>

#include <Windows.h>
#include <cassert>
#include <filesystem>

namespace Renderer
{

namespace
{
    std::string ResolveAssetPath(const std::string& relativePath)
    {
        char buffer[MAX_PATH];
        GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        std::filesystem::path root = std::filesystem::path(buffer)
            .parent_path().parent_path().parent_path().parent_path().parent_path();
        return root.string() + relativePath;
    }
} // namespace

RHI::Format VertexDescLoader::ParseFormat(const std::string& s)
{
    if (s == "r32_float")           return RHI::Format::R32Float;
    if (s == "r32g32_float")        return RHI::Format::R32G32Float;
    if (s == "r32g32b32_float")     return RHI::Format::R32G32B32Float;
    if (s == "r32g32b32a32_float")  return RHI::Format::R32G32B32A32Float;

    LOGERROR("VertexDescLoader: unknown format '", s, "'");
    assert(false && "Unknown format in .vdes file");
    return RHI::Format::Undefined;
}

RHI::VertexInputRate VertexDescLoader::ParseInputRate(const std::string& s)
{
    if (s == "per_vertex")   return RHI::VertexInputRate::PerVertex;
    if (s == "per_instance") return RHI::VertexInputRate::PerInstance;

    LOGERROR("VertexDescLoader: unknown input rate '", s, "'");
    assert(false && "Unknown input rate in .vdes file");
    return RHI::VertexInputRate::PerVertex;
}

VertexFileDesc VertexDescLoader::Load(const std::string& assetRelativePath)
{
    const std::string fullPath = ResolveAssetPath(assetRelativePath);

    YAML::Node root;
    try
    {
        root = YAML::LoadFile(fullPath);
    }
    catch (const YAML::Exception& e)
    {
        LOGERROR("VertexDescLoader: failed to load '", fullPath, "': ", e.what());
        assert(false && "Vertex description file not found or malformed.");
    }

    VertexFileDesc desc;

    if (const YAML::Node& bindingsNode = root["bindings"])
    {
        for (const YAML::Node& b : bindingsNode)
        {
            RHI::VertexBinding binding;
            binding.m_Binding   = b["binding"].as<uint32_t>();
            binding.m_Stride    = b["stride"].as<uint32_t>();
            binding.m_InputRate = b["input_rate"]
                ? ParseInputRate(b["input_rate"].as<std::string>())
                : RHI::VertexInputRate::PerVertex;
            desc.m_Bindings.push_back(binding);
        }
    }

    if (const YAML::Node& attrsNode = root["attributes"])
    {
        for (const YAML::Node& a : attrsNode)
        {
            RHI::VertexAttribute attr;
            attr.m_Location = a["location"].as<uint32_t>();
            attr.m_Binding  = a["binding"].as<uint32_t>();
            attr.m_Format   = ParseFormat(a["format"].as<std::string>());
            attr.m_Offset   = a["offset"] ? a["offset"].as<uint32_t>() : 0u;
            desc.m_Attributes.push_back(attr);
        }
    }

    return desc;
}

} // namespace Renderer

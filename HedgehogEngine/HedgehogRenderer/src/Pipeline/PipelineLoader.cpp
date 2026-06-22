#include "PipelineLoader.hpp"

#include "Logger/api/Logger.hpp"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cassert>
#include <sstream>

namespace Renderer
{

RHI::DescriptorType PipelineLoader::ParseDescriptorType(const std::string& s)
{
    if (s == "uniform_buffer")         return RHI::DescriptorType::UniformBuffer;
    if (s == "storage_buffer")         return RHI::DescriptorType::StorageBuffer;
    if (s == "combined_image_sampler") return RHI::DescriptorType::CombinedImageSampler;
    if (s == "storage_image")          return RHI::DescriptorType::StorageImage;
    if (s == "input_attachment")       return RHI::DescriptorType::InputAttachment;

    LOGERROR("PipelineLoader: unknown descriptor type '", s, "'");
    assert(false && "Unknown descriptor type in .pl file");
    return RHI::DescriptorType::UniformBuffer;
}

RHI::ShaderStage PipelineLoader::ParseShaderStage(const std::string& s)
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
            LOGERROR("PipelineLoader: unknown shader stage '", token, "'");
            assert(false && "Unknown shader stage in .pl file");
        }
    }
    return result;
}

PipelineFileDesc PipelineLoader::Load(const std::string& virtualPath,
                                       const FS::FileSystemManager& fileSystem)
{
    const auto text = fileSystem.ReadTextFile(virtualPath);
    if (!text)
    {
        LOGERROR("PipelineLoader: failed to read '", virtualPath, "'");
        assert(false && "Pipeline file not found or malformed.");
    }

    PipelineFileDesc desc;

    try
    {
        YAML::Node root = YAML::Load(*text);

        // descriptor_sets: list of sets; position in list = set index in pipeline layout.
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
                        binding.m_Stages  = ParseShaderStage(b["stage"].as<std::string>());
                        bindings.push_back(binding);
                    }
                }
                desc.m_DescriptorSets.push_back(std::move(bindings));
            }
        }

        // push_constants: list of ranges.
        if (const YAML::Node& pcs = root["push_constants"])
        {
            for (const YAML::Node& pc : pcs)
            {
                RHI::PushConstantRange range;
                range.m_Stages = ParseShaderStage(pc["stage"].as<std::string>());
                range.m_Offset = pc["offset"] ? pc["offset"].as<uint32_t>() : 0u;
                range.m_Size   = pc["size"].as<uint32_t>();
                desc.m_PushConstants.push_back(range);
            }
        }
    }
    catch (const YAML::Exception& e)
    {
        LOGERROR("PipelineLoader: failed to parse '", virtualPath, "': ", e.what());
        assert(false && "Pipeline file not found or malformed.");
    }

    return desc;
}

std::vector<RHI::PoolSize> PipelineLoader::MakePoolSizes(
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

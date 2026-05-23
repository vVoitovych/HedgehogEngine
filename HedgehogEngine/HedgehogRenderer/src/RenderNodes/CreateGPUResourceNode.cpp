#include "CreateGPUResourceNode.hpp"

#include "RenderGraph/RenderGraph.hpp"

#include "yaml-cpp/yaml.h"

#include "Logger/api/Logger.hpp"

#include <string>

namespace
{
    RHI::Format ParseFormat(const std::string& str)
    {
        if (str == "depth32")  return RHI::Format::D32Float;
        if (str == "depth16")  return RHI::Format::D16Unorm;
        if (str == "rgba8")    return RHI::Format::R8G8B8A8Unorm;
        if (str == "rgba16f")  return RHI::Format::R16G16B16A16Float;
        if (str == "rgba16")   return RHI::Format::R16G16B16A16Unorm;
        if (str == "r16f")     return RHI::Format::R16Float;
        if (str == "r32f")     return RHI::Format::R32Float;
        LOGERROR("CreateGPUResourceNode: unknown format '", str, "', defaulting to rgba8.");
        return RHI::Format::R8G8B8A8Unorm;
    }

    uint32_t ParseExtent(const YAML::Node& resNode, const std::string& field)
    {
        const std::string val = resNode[field].as<std::string>();
        if (val == "backbuffer")
            return Renderer::BACKBUFFER_EXTENT;
        return static_cast<uint32_t>(std::stoul(val));
    }
}

namespace Renderer
{
    void CreateGPUResourceNode::SetConfig(const YAML::Node& entry)
    {
        if (!entry["resources"])
            return;

        for (const YAML::Node& res : entry["resources"])
        {
            const std::string name = res["name"].as<std::string>();
            const std::string kind = res["kind"].as<std::string>();

            if (kind == "texture")
            {
                GraphTextureDesc desc;
                desc.m_Name   = name;
                desc.m_Format = ParseFormat(res["format"].as<std::string>());
                desc.m_Width  = ParseExtent(res, "width");
                desc.m_Height = ParseExtent(res, "height");
                m_TextureDescs.push_back(std::move(desc));
            }
            else if (kind == "buffer")
            {
                GraphBufferDesc desc;
                desc.m_Name = name;
                desc.m_Size = res["size"].as<size_t>();
                m_BufferDescs.push_back(std::move(desc));
            }
            else
            {
                LOGERROR("CreateGPUResourceNode: unknown resource kind '", kind,
                         "' for resource '", name, "'.");
            }
        }
    }

    void CreateGPUResourceNode::Setup(RenderGraph& graph)
    {
        m_Desc.name = "CreateGPUResourceNode";

        for (const auto& desc : m_TextureDescs)
            graph.DeclareGraphTexture(desc);

        for (const auto& desc : m_BufferDescs)
            graph.DeclareGraphBuffer(desc);
    }
}

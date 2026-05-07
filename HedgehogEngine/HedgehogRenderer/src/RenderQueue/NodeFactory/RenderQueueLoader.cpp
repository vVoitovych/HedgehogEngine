#include "RenderQueueLoader.hpp"

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

    std::vector<NodeConfig> RenderQueueLoader::Load(const std::string& assetRelativePath)
    {
        const std::string fullPath = ResolveAssetPath(assetRelativePath);

        YAML::Node root;
        try
        {
            root = YAML::LoadFile(fullPath);
        }
        catch (const YAML::Exception& e)
        {
            LOGERROR("RenderQueueLoader: failed to load '", fullPath, "': ", e.what());
            assert(false && "Render queue file not found or malformed.");
            return {};
        }

        const YAML::Node& passes = root["passes"];
        if (!passes || !passes.IsSequence())
        {
            LOGERROR("RenderQueueLoader: '", fullPath, "' has no 'passes' sequence.");
            assert(false && "Render queue file missing 'passes' key.");
            return {};
        }

        std::vector<NodeConfig> configs;
        configs.reserve(passes.size());

        for (const YAML::Node& entry : passes)
        {
            NodeConfig cfg;
            if (entry["name"])     cfg.m_Name       = entry["name"].as<std::string>();
            if (entry["type"])     cfg.m_Type       = entry["type"].as<std::string>();
            if (entry["shader"])   cfg.m_Shader     = entry["shader"].as<std::string>();
            if (entry["resource"]) cfg.m_Resource   = entry["resource"].as<std::string>();
            if (entry["from"])     cfg.m_FromLayout = entry["from"].as<std::string>();
            if (entry["to"])       cfg.m_ToLayout   = entry["to"].as<std::string>();

            if (const YAML::Node& att = entry["attachments"])
            {
                auto parseAttachment = [](const YAML::Node& n) -> AttachmentConfig
                {
                    AttachmentConfig ac;
                    if (n["resource"]) ac.m_Resource      = n["resource"].as<std::string>();
                    if (n["load"])     ac.m_LoadOp        = n["load"].as<std::string>();
                    if (n["store"])    ac.m_StoreOp       = n["store"].as<std::string>();
                    if (n["initial"])  ac.m_InitialLayout = n["initial"].as<std::string>();
                    if (n["final"])    ac.m_FinalLayout   = n["final"].as<std::string>();
                    return ac;
                };

                if (const YAML::Node& color = att["color"])
                {
                    if (color.IsSequence())
                    {
                        for (const auto& c : color)
                            cfg.m_ColorAttachments.push_back(parseAttachment(c));
                    }
                    else
                    {
                        cfg.m_ColorAttachments.push_back(parseAttachment(color));
                    }
                }

                if (const YAML::Node& depth = att["depth"])
                    cfg.m_DepthAttachment = parseAttachment(depth);
            }

            if (const YAML::Node& inputs = entry["inputs"])
            {
                if (inputs.IsSequence())
                {
                    for (const auto& inp : inputs)
                        cfg.m_InputResources.push_back(inp.as<std::string>());
                }
            }

            configs.push_back(std::move(cfg));
        }

        return configs;
    }

} // namespace Renderer

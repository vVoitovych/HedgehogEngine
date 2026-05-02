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
            configs.push_back(std::move(cfg));
        }

        return configs;
    }

} // namespace Renderer

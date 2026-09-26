#include "GraphLayoutFile.hpp"

#include "Logger/api/Logger.hpp"

#include <yaml-cpp/yaml.h>

#include <cmath>
#include <fstream>

namespace Editor
{
    std::filesystem::path GetGraphLayoutFile(const std::filesystem::path& graphFile)
    {
        std::filesystem::path layout = graphFile;
        layout += ".layout";
        return layout;
    }

    std::optional<GraphLayout> LoadGraphLayout(const std::filesystem::path& layoutFile)
    {
        std::error_code error;
        if (!std::filesystem::exists(layoutFile, error))
            return std::nullopt;

        try
        {
            const YAML::Node root  = YAML::LoadFile(layoutFile.string());
            const YAML::Node nodes = root["nodes"];
            GraphLayout layout;
            if (nodes && nodes.IsMap())
            {
                for (const auto& entry : nodes)
                {
                    const YAML::Node position = entry.second;
                    if (!position.IsSequence() || position.size() != 2)
                        continue;
                    layout[entry.first.as<std::string>()] = { position[0].as<float>(), position[1].as<float>() };
                }
            }
            return layout;
        }
        catch (const YAML::Exception& exception)
        {
            LOGWARNING("Graph layout ", layoutFile.string(), " could not be read (", exception.what(),
                       "); using the automatic layout.");
            return std::nullopt;
        }
    }

    bool SaveGraphLayout(const std::filesystem::path& layoutFile, const GraphLayout& layout)
    {
        YAML::Emitter out;
        out << YAML::BeginMap << YAML::Key << "nodes" << YAML::Value << YAML::BeginMap;
        for (const auto& [key, position] : layout)
        {
            // Whole pixels: sub-pixel drift would rewrite the file with no visible change.
            out << YAML::Key << key << YAML::Value << YAML::Flow << YAML::BeginSeq
                << std::round(position.X) << std::round(position.Y) << YAML::EndSeq;
        }
        out << YAML::EndMap << YAML::EndMap;

        std::ofstream stream(layoutFile, std::ios::binary | std::ios::trunc);
        if (stream)
            stream << out.c_str() << "\n";
        if (!stream)
        {
            LOGWARNING("Graph layout ", layoutFile.string(), " could not be written.");
            return false;
        }
        return true;
    }
}

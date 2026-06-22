#include "MaterialSerializer.hpp"

#include "HedgehogEngine/api/Containers/MaterialData.hpp"

#include "Logger/api/Logger.hpp"

#include "yaml-cpp/yaml.h"

#include <fstream>
#include <string_view>

namespace HedgehogEngine
{
    void MaterialSerializer::Serialize(const MaterialData& material, const std::string& materialPath)
    {
        LOGINFO("Serialize material: ", materialPath);

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Type"         << YAML::Value << static_cast<size_t>(material.type);
        out << YAML::Key << "BaseColor"    << YAML::Value << material.baseColor;
        out << YAML::Key << "Transparency" << YAML::Value << material.transparency;
        out << YAML::EndMap;

        std::ofstream fout(materialPath);
        if (!fout.is_open())
        {
            LOGERROR("MaterialSerializer::Serialize: failed to open '", materialPath, "' for writing.");
            return;
        }
        fout << out.c_str();
    }

    void MaterialSerializer::Deserialize(MaterialData& material,
                                          const std::string& virtualPath,
                                          const FS::FileSystemManager& fileSystem)
    {
        LOGINFO("Deserialize material: ", virtualPath);

        const auto text = fileSystem.ReadTextFile(virtualPath);
        if (!text)
        {
            LOGERROR("Failed to read material file: ", virtualPath);
            return;
        }

        try
        {
            YAML::Node data = YAML::Load(*text);

            // Strip "assets://" prefix to obtain the asset-relative path stored in MaterialData.
            constexpr std::string_view assetsPrefix = "assets://";
            material.path         = virtualPath.substr(assetsPrefix.size());
            material.type         = static_cast<MaterialType>(data["Type"].as<size_t>());
            material.baseColor    = data["BaseColor"].as<std::string>();
            material.transparency = data["Transparency"].as<float>();
        }
        catch (const YAML::Exception& e)
        {
            LOGERROR("Failed to parse material: ", virtualPath, " with error: ", e.what());
            return;
        }
    }
}

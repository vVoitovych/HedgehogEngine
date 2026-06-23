#include "MaterialSerializer.hpp"

#include "HedgehogEngine/api/Containers/MaterialData.hpp"

#include "Logger/api/Logger.hpp"

#include "yaml-cpp/yaml.h"

#include <cassert>
#include <string_view>

namespace HedgehogEngine
{
    void MaterialSerializer::Serialize(const MaterialData& material,
                                        const std::string& virtualPath,
                                        const FS::FileSystemManager& fileSystem)
    {
        LOGINFO("Serialize material: ", virtualPath);

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Type"         << YAML::Value << static_cast<size_t>(material.type);
        out << YAML::Key << "BaseColor"    << YAML::Value << material.baseColor;
        out << YAML::Key << "Transparency" << YAML::Value << material.transparency;
        out << YAML::EndMap;

        if (!fileSystem.WriteTextFile(virtualPath, out.c_str()))
            LOGERROR("MaterialSerializer::Serialize: failed to write '", virtualPath, "'.");
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

            constexpr std::string_view assetsPrefix = "assets://";
            assert(virtualPath.substr(0, assetsPrefix.size()) == assetsPrefix
                && "MaterialSerializer: virtualPath must use assets:// alias");
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

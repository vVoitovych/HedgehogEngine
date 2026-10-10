#include "HedgehogEngine/api/Containers/MaterialSerializer.hpp"

#include "HedgehogEngine/api/Containers/MaterialData.hpp"

#include "Logger/api/Logger.hpp"

#include "yaml-cpp/yaml.h"

#include <cassert>
#include <cmath>
#include <string_view>

namespace HedgehogEngine
{
    namespace
    {
        void WarnKey(const std::string& source, const char* key, const char* what)
        {
            LOGWARNING("[Material] " + source + ": " + key + " is not " + what + "; its default is kept.");
        }

        bool ReadFloat(const YAML::Node& node, float& out)
        {
            try
            {
                const float value = node.as<float>();
                if (!std::isfinite(value))
                    return false;
                out = value;
                return true;
            }
            catch (const YAML::Exception&)
            {
                return false;
            }
        }

        void ReadNumber(const YAML::Node& data, const char* key, float& out, const std::string& source)
        {
            if (const YAML::Node node = data[key]; node && !ReadFloat(node, out))
                WarnKey(source, key, "a number");
        }

        void ReadPath(const YAML::Node& data, const char* key, std::string& out, const std::string& source)
        {
            const YAML::Node node = data[key];
            if (!node)
                return;
            if (!node.IsScalar())
            {
                WarnKey(source, key, "a path");
                return;
            }
            out = node.as<std::string>();
        }

        template <size_t N>
        void ReadVector(const YAML::Node& data, const char* key, HM::Vector<N>& out, const std::string& source)
        {
            const YAML::Node node = data[key];
            if (!node)
                return;
            HM::Vector<N> value = out;
            bool          valid = node.IsSequence() && node.size() == N;
            for (size_t i = 0; valid && i < N; ++i)
                valid = ReadFloat(node[i], value[i]);
            if (!valid)
            {
                WarnKey(source, key, N == 3 ? "three numbers" : "four numbers");
                return;
            }
            out = value;
        }

        template <size_t N>
        void WriteVector(YAML::Emitter& out, const char* key, const HM::Vector<N>& value)
        {
            out << YAML::Key << key << YAML::Value << YAML::Flow << YAML::BeginSeq;
            for (size_t i = 0; i < N; ++i)
                out << value[i];
            out << YAML::EndSeq;
        }
    }

    std::string MaterialSerializer::WriteText(const MaterialData& material)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Type" << YAML::Value << static_cast<size_t>(material.type);
        out << YAML::Key << "BaseColor" << YAML::Value << material.baseColor;
        WriteVector(out, "BaseColorFactor", material.baseColorFactor);
        out << YAML::Key << "Transparency" << YAML::Value << material.transparency;
        out << YAML::Key << "AlphaCutoff" << YAML::Value << material.alphaCutoff;
        out << YAML::Key << "DoubleSided" << YAML::Value << material.doubleSided;
        out << YAML::Key << "Metallic" << YAML::Value << material.metallic;
        out << YAML::Key << "Roughness" << YAML::Value << material.roughness;
        out << YAML::Key << "MetallicRoughnessMap" << YAML::Value << material.metallicRoughnessMap;
        out << YAML::Key << "NormalMap" << YAML::Value << material.normalMap;
        out << YAML::Key << "NormalScale" << YAML::Value << material.normalScale;
        out << YAML::Key << "OcclusionMap" << YAML::Value << material.occlusionMap;
        out << YAML::Key << "OcclusionStrength" << YAML::Value << material.occlusionStrength;
        out << YAML::Key << "EmissiveMap" << YAML::Value << material.emissiveMap;
        WriteVector(out, "EmissiveFactor", material.emissiveFactor);
        out << YAML::EndMap;
        return out.c_str();
    }

    bool MaterialSerializer::ReadText(MaterialData& material, const std::string& text, const std::string& source)
    {
        YAML::Node data;
        try
        {
            data = YAML::Load(text);
        }
        catch (const YAML::Exception& e)
        {
            LOGERROR("[Material] " + source + " is not YAML: " + e.what());
            return false;
        }
        if (!data.IsMap())
        {
            LOGERROR("[Material] " + source + " is not a map of material keys.");
            return false;
        }

        if (const YAML::Node type = data["Type"])
        {
            float value = 0.0f;
            if (ReadFloat(type, value) && value >= 0.0f && value <= 2.0f && value == std::floor(value))
                material.type = static_cast<MaterialType>(static_cast<int>(value));
            else
                WarnKey(source, "Type", "0, 1 or 2");
        }
        ReadPath(data, "BaseColor", material.baseColor, source);
        ReadVector(data, "BaseColorFactor", material.baseColorFactor, source);
        ReadNumber(data, "Transparency", material.transparency, source);
        if (const YAML::Node cutoff = data["AlphaCutoff"])
        {
            float value = 0.0f;
            if (ReadFloat(cutoff, value) && value >= 0.0f && value <= 1.0f)
                material.alphaCutoff = value;
            else
                WarnKey(source, "AlphaCutoff", "a number from 0 to 1");
        }
        if (const YAML::Node doubleSided = data["DoubleSided"])
        {
            try
            {
                material.doubleSided = doubleSided.as<bool>();
            }
            catch (const YAML::Exception&)
            {
                WarnKey(source, "DoubleSided", "true or false");
            }
        }
        ReadNumber(data, "Metallic", material.metallic, source);
        ReadNumber(data, "Roughness", material.roughness, source);
        ReadPath(data, "MetallicRoughnessMap", material.metallicRoughnessMap, source);
        ReadPath(data, "NormalMap", material.normalMap, source);
        ReadNumber(data, "NormalScale", material.normalScale, source);
        ReadPath(data, "OcclusionMap", material.occlusionMap, source);
        ReadNumber(data, "OcclusionStrength", material.occlusionStrength, source);
        ReadPath(data, "EmissiveMap", material.emissiveMap, source);
        ReadVector(data, "EmissiveFactor", material.emissiveFactor, source);
        return true;
    }

    void MaterialSerializer::Serialize(const MaterialData& material,
                                        const std::string& virtualPath,
                                        const FS::FileSystemManager& fileSystem)
    {
        LOGINFO("Serialize material: ", virtualPath);
        if (!fileSystem.WriteTextFile(virtualPath, WriteText(material)))
            LOGERROR("MaterialSerializer::Serialize: failed to write '", virtualPath, "'.");
    }

    void MaterialSerializer::Deserialize(MaterialData& material,
                                          const std::string& virtualPath,
                                          const FS::FileSystemManager& fileSystem)
    {
        LOGINFO("Deserialize material: ", virtualPath);

        constexpr std::string_view assetsPrefix = "assets://";
        assert(virtualPath.substr(0, assetsPrefix.size()) == assetsPrefix
            && "MaterialSerializer: virtualPath must use assets:// alias");
        material.path = virtualPath.substr(assetsPrefix.size());

        const auto text = fileSystem.ReadTextFile(virtualPath);
        if (!text)
        {
            LOGERROR("Failed to read material file: ", virtualPath);
            return;
        }
        (void)ReadText(material, *text, virtualPath);
    }
}

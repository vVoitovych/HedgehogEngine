#include "HedgehogEngine/api/Assets/GltfMaterialImport.hpp"

#include "HedgehogEngine/api/Containers/MaterialData.hpp"
#include "HedgehogEngine/api/Containers/MaterialSerializer.hpp"

#include "ContentLoader/api/MaterialLoader.hpp"
#include "FileSystem/api/PathUtils.hpp"
#include "Logger/api/Logger.hpp"

#include <cctype>

namespace HedgehogEngine
{
    namespace
    {
        constexpr std::string_view ASSETS_PREFIX = "assets://";

        // A map path as the shipped materials name theirs: under assets:// without the prefix.
        std::string ToMaterialMapPath(const std::string& path)
        {
            return path.starts_with(ASSETS_PREFIX) ? path.substr(ASSETS_PREFIX.size()) : path;
        }
    }

    std::string MakeImportedMaterialPath(const std::string& gltfPath, const std::string& materialName)
    {
        std::string path = FS::ToAssetVirtualPath(gltfPath);
        for (char& c : path)
            c = c == '\\' ? '/' : c;
        const size_t slash = path.find_last_of('/');
        const size_t dot   = path.find_last_of('.');
        const size_t stem  = slash == std::string::npos ? 0 : slash + 1;
        const std::string folder = path.substr(0, stem);
        const std::string name   = path.substr(stem, dot == std::string::npos || dot < stem ? std::string::npos : dot - stem);

        std::string sanitised = materialName;
        for (char& c : sanitised)
        {
            const auto u = static_cast<unsigned char>(c);
            if (!std::isalnum(u) && c != '_' && c != '-')
                c = '_';
        }
        return folder + name + "_" + sanitised + ".material";
    }

    MaterialData MakeMaterialData(const ContentLoader::LoadedMaterial& material)
    {
        MaterialData data;
        data.type                 = MaterialType::Opaque;
        data.baseColor            = ToMaterialMapPath(material.BaseColorMap);
        data.baseColorFactor      = material.BaseColorFactor;
        data.metallic             = material.Metallic;
        data.roughness            = material.Roughness;
        data.metallicRoughnessMap = ToMaterialMapPath(material.MetallicRoughnessMap);
        data.normalMap            = ToMaterialMapPath(material.NormalMap);
        data.normalScale          = material.NormalScale;
        data.occlusionMap         = ToMaterialMapPath(material.OcclusionMap);
        data.occlusionStrength    = material.OcclusionStrength;
        data.emissiveMap          = ToMaterialMapPath(material.EmissiveMap);
        data.emissiveFactor       = material.EmissiveFactor;
        return data;
    }

    GltfMaterialImportResult ImportGltfMaterials(const std::string& gltfPath, const FS::FileSystemManager& fileSystem)
    {
        GltfMaterialImportResult result;
        const std::string        virtualPath = FS::ToAssetVirtualPath(gltfPath);
        const auto               materials   = ContentLoader::LoadGltfMaterials(virtualPath, fileSystem);
        if (!materials)
        {
            result.Error = virtualPath + " cannot be read as a glTF file.";
            return result;
        }

        for (const ContentLoader::LoadedMaterial& material : *materials)
        {
            const std::string path = MakeImportedMaterialPath(virtualPath, material.Name);
            result.Paths.push_back(path);
            if (fileSystem.Exists(path))
                continue; // imported before, perhaps edited since: never overwritten
            if (fileSystem.WriteTextFile(path, MaterialSerializer::WriteText(MakeMaterialData(material))))
            {
                LOGINFO("[Material] Imported " + path + " from " + virtualPath + ".");
                result.Written.push_back(path);
            }
            else
            {
                LOGERROR("[Material] " + path + " cannot be written.");
            }
        }
        return result;
    }
}

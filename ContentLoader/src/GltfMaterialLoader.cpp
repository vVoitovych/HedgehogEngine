#include "api/MaterialLoader.hpp"

#include "GltfModel.hpp"

#include "FileSystem/api/PathUtils.hpp"
#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <cctype>
#include <vector>

namespace ContentLoader
{
    namespace
    {
        bool IsGltfFile(const std::string& path)
        {
            std::string lower = path;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return lower.ends_with(".gltf") || lower.ends_with(".glb");
        }

        // A URI's percent escapes decoded ("My%20Texture.png" is "My Texture.png").
        std::string DecodeUri(const std::string& uri)
        {
            std::string decoded;
            for (size_t i = 0; i < uri.size(); ++i)
            {
                if (uri[i] == '%' && i + 2 < uri.size() && std::isxdigit(static_cast<unsigned char>(uri[i + 1]))
                    && std::isxdigit(static_cast<unsigned char>(uri[i + 2])))
                {
                    decoded.push_back(static_cast<char>(std::stoi(uri.substr(i + 1, 2), nullptr, 16)));
                    i += 2;
                }
                else
                {
                    decoded.push_back(uri[i]);
                }
            }
            return decoded;
        }

        // uri next to the virtual path file, with "." and ".." segments folded (a ".." past the mount's
        // root is dropped) and backslashes as slashes.
        std::string ResolveNextTo(const std::string& file, const std::string& uri)
        {
            const size_t      mountEnd = file.find("://");
            const size_t      rootEnd  = mountEnd == std::string::npos ? 0 : mountEnd + 3;
            const std::string mount    = file.substr(0, rootEnd);
            std::string       joined   = file.substr(rootEnd);
            const size_t      slash    = joined.find_last_of("/\\");
            joined = (slash == std::string::npos ? std::string() : joined.substr(0, slash + 1)) + uri;
            std::replace(joined.begin(), joined.end(), '\\', '/');

            std::vector<std::string> segments;
            size_t                   start = 0;
            while (start <= joined.size())
            {
                const size_t      end     = std::min(joined.find('/', start), joined.size());
                const std::string segment = joined.substr(start, end - start);
                if (segment == "..")
                {
                    if (!segments.empty())
                        segments.pop_back();
                }
                else if (!segment.empty() && segment != ".")
                {
                    segments.push_back(segment);
                }
                start = end + 1;
            }
            std::string result = mount;
            for (size_t i = 0; i < segments.size(); ++i)
                result += (i ? "/" : "") + segments[i];
            return result;
        }

        // The virtual path of a texture's image, or empty (with one warning) when it has none on disk.
        std::string ReadMap(const tinygltf::Model& model, int textureIndex, int texCoord, const std::string& virtualPath,
                            const std::string& material, const char* slot)
        {
            if (textureIndex < 0)
                return {};
            // One string, since the Logger puts a space between its arguments.
            const std::string where = "[Material] " + virtualPath + ": material '" + material + "' " + slot;
            if (textureIndex >= static_cast<int>(model.textures.size()))
            {
                LOGWARNING(where + " names texture " + std::to_string(textureIndex) + ", which does not exist; it is skipped.");
                return {};
            }
            const int source = model.textures[textureIndex].source;
            if (source < 0 || source >= static_cast<int>(model.images.size()))
            {
                LOGWARNING(where + " has no image; it is skipped.");
                return {};
            }
            const tinygltf::Image& image = model.images[source];
            if (image.uri.empty() || image.uri.starts_with("data:"))
            {
                LOGWARNING(where + " image is embedded in the file; it is skipped.");
                return {};
            }
            if (texCoord != 0)
                LOGWARNING(where + " reads texture coordinates " + std::to_string(texCoord) + "; the first are used.");
            return ResolveNextTo(virtualPath, DecodeUri(image.uri));
        }
    }

    std::optional<std::vector<LoadedMaterial>> LoadGltfMaterials(const std::string&           fileName,
                                                                const FS::FileSystemManager& fileSystem)
    {
        const std::string virtualPath = FS::ToAssetVirtualPath(fileName);
        const auto        physPath    = fileSystem.ResolvePhysical(virtualPath);
        if (!physPath)
        {
            LOGERROR("Cannot resolve material path: " + virtualPath);
            return std::nullopt;
        }
        if (!IsGltfFile(virtualPath))
        {
            LOGERROR("Unsupported material file format: " + virtualPath);
            return std::nullopt;
        }

        tinygltf::Model model;
        if (!LoadGltfModel(physPath->string(), model, false))
            return std::nullopt;

        std::vector<LoadedMaterial> materials;
        materials.reserve(model.materials.size());
        for (size_t i = 0; i < model.materials.size(); ++i)
        {
            const tinygltf::Material&             source = model.materials[i];
            const tinygltf::PbrMetallicRoughness& pbr    = source.pbrMetallicRoughness;
            const std::string                     name   = source.name.empty() ? "Material " + std::to_string(i) : source.name;

            LoadedMaterial material;
            material.Name = name;
            if (pbr.baseColorFactor.size() == 4)
                material.BaseColorFactor = HM::Vector4(static_cast<float>(pbr.baseColorFactor[0]), static_cast<float>(pbr.baseColorFactor[1]),
                                                       static_cast<float>(pbr.baseColorFactor[2]), static_cast<float>(pbr.baseColorFactor[3]));
            material.Metallic          = static_cast<float>(pbr.metallicFactor);
            material.Roughness         = static_cast<float>(pbr.roughnessFactor);
            material.NormalScale       = static_cast<float>(source.normalTexture.scale);
            material.OcclusionStrength = static_cast<float>(source.occlusionTexture.strength);
            if (source.emissiveFactor.size() == 3)
                material.EmissiveFactor = HM::Vector3(static_cast<float>(source.emissiveFactor[0]), static_cast<float>(source.emissiveFactor[1]),
                                                      static_cast<float>(source.emissiveFactor[2]));

            if (source.alphaMode == "MASK")
                material.AlphaMode = LoadedAlphaMode::Mask;
            else if (source.alphaMode == "BLEND")
                material.AlphaMode = LoadedAlphaMode::Blend;
            else if (source.alphaMode != "OPAQUE")
                LOGWARNING("[Material] " + virtualPath + ": material '" + name + "' has alpha mode '" + source.alphaMode
                           + "'; it is read as OPAQUE.");
            material.AlphaCutoff = static_cast<float>(source.alphaCutoff);
            material.DoubleSided = source.doubleSided;

            material.BaseColorMap = ReadMap(model, pbr.baseColorTexture.index, pbr.baseColorTexture.texCoord, virtualPath, name,
                                            "base colour");
            material.MetallicRoughnessMap = ReadMap(model, pbr.metallicRoughnessTexture.index, pbr.metallicRoughnessTexture.texCoord,
                                                    virtualPath, name, "metallic-roughness");
            material.NormalMap = ReadMap(model, source.normalTexture.index, source.normalTexture.texCoord, virtualPath, name, "normal");
            material.OcclusionMap = ReadMap(model, source.occlusionTexture.index, source.occlusionTexture.texCoord, virtualPath, name,
                                            "occlusion");
            material.EmissiveMap = ReadMap(model, source.emissiveTexture.index, source.emissiveTexture.texCoord, virtualPath, name,
                                           "emissive");
            materials.push_back(std::move(material));
        }
        return materials;
    }
}

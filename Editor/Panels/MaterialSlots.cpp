#include "MaterialSlots.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

namespace Editor
{
    namespace
    {
        using HedgehogEngine::MaterialTextureSlot;

        constexpr std::array<MaterialSlotInfo, HedgehogEngine::MATERIAL_TEXTURE_SLOT_COUNT> SLOTS = { {
            { MaterialTextureSlot::BaseColor, "Base colour map", "##BaseColorMap" },
            { MaterialTextureSlot::MetallicRoughness, "Metallic-roughness map", "##MetallicRoughnessMap" },
            { MaterialTextureSlot::Normal, "Normal map", "##NormalMap" },
            { MaterialTextureSlot::Occlusion, "Occlusion map", "##OcclusionMap" },
            { MaterialTextureSlot::Emissive, "Emissive map", "##EmissiveMap" },
        } };

        constexpr std::string_view ASSETS_PREFIX = "assets://";
    }

    std::span<const MaterialSlotInfo> GetMaterialSlots()
    {
        return SLOTS;
    }

    std::optional<std::string> ToMaterialTexturePath(const std::string& virtualPath)
    {
        if (!virtualPath.starts_with(ASSETS_PREFIX))
            return std::nullopt;
        std::string path = virtualPath.substr(ASSETS_PREFIX.size());
        std::replace(path.begin(), path.end(), '\\', '/');
        return path;
    }

    bool IsGltfPath(const std::string& path)
    {
        std::string lower = path;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lower.ends_with(".gltf") || lower.ends_with(".glb");
    }
}

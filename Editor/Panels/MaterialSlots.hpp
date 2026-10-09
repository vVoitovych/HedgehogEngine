#pragma once

#include "HedgehogEngine/api/Containers/MaterialData.hpp"

#include <optional>
#include <span>
#include <string>

// What the material inspector (MaterialFields) needs that is not drawing, so EditorTest covers it.
namespace Editor
{
    // A material's map slot as the inspector shows it: its label and its ImGui id.
    struct MaterialSlotInfo
    {
        HedgehogEngine::MaterialTextureSlot Slot;
        const char*                         Label;
        const char*                         Id;
    };

    // Every slot once, in the order the inspector lists them: base colour, metallic-roughness,
    // normal, occlusion, emissive (glTF's order of the maps that shade with them).
    [[nodiscard]] std::span<const MaterialSlotInfo> GetMaterialSlots();

    // A texture's path as a material names it: under assets:// without the prefix ("Textures/a.png"
    // for "assets://Textures/a.png"), backslashes as slashes; nullopt for a path on another mount,
    // which a material cannot name.
    [[nodiscard]] std::optional<std::string> ToMaterialTexturePath(const std::string& virtualPath);

    // Whether path is a glTF model (.gltf or .glb, any case), whose materials can be imported.
    [[nodiscard]] bool IsGltfPath(const std::string& path);
}

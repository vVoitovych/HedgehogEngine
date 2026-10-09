#pragma once

#include <cstddef>

namespace FS
{
    class FileSystemManager;
}

namespace HedgehogEngine
{
    class MaterialContainer;
    class TextureContainer;
}

namespace Editor
{
    // Draws a material's PBR fields as property rows, inside the caller's property table: its type
    // (and transparency when transparent), base colour factor, metallic, roughness, normal scale,
    // occlusion strength and emissive colour, then one row per map slot (MaterialSlots): a drop-down
    // of the known textures that also takes a texture dropped from the Content panel, then Load...
    // and Clear (back to the slot's neutral default). Every edit marks the material dirty, so the
    // renderer shows it the next frame; Save material writes it.
    void DrawMaterialFields(HedgehogEngine::MaterialContainer& materials, const HedgehogEngine::TextureContainer& textures,
                            size_t index, const FS::FileSystemManager& fileSystem);
}

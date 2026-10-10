#pragma once

#include "HedgehogMath/api/Vector.hpp"

#include <string>

namespace ContentLoader
{
    // glTF's alphaMode.
    enum class LoadedAlphaMode
    {
        Opaque, // OPAQUE: alpha ignored
        Mask,   // MASK: below AlphaCutoff discarded
        Blend   // BLEND: blended over what is behind
    };

    // One glTF material in the metallic-roughness model, as plain data (LoadGltfMaterials). Values a
    // file leaves out have glTF's defaults; a map is the virtual path of its image, resolved next to
    // the glTF file, or empty for none.
    struct LoadedMaterial
    {
        std::string Name;

        HM::Vector4 BaseColorFactor   = HM::Vector4(1.0f, 1.0f, 1.0f, 1.0f); // linear RGBA
        float       Metallic          = 1.0f;
        float       Roughness         = 1.0f;
        float       NormalScale       = 1.0f;
        float       OcclusionStrength = 1.0f;
        HM::Vector3 EmissiveFactor    = HM::Vector3(0.0f, 0.0f, 0.0f); // linear RGB

        LoadedAlphaMode AlphaMode   = LoadedAlphaMode::Opaque;
        float           AlphaCutoff = 0.5f;
        bool            DoubleSided = false;

        std::string BaseColorMap;
        std::string MetallicRoughnessMap; // G roughness, B metallic
        std::string NormalMap;
        std::string OcclusionMap;         // R
        std::string EmissiveMap;
    };
}

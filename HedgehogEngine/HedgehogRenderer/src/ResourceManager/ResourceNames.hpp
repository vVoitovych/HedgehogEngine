#pragma once

#include <string_view>

namespace Renderer
{
    namespace ResourceNames
    {
        // Resolves to the swapchain image at the current back-buffer index.
        // Cannot be created or destroyed via the ResourceManager texture API.
        inline constexpr std::string_view SWAP_CHAIN_BACK_BUFFER = "SWAP_CHAIN_BACK_BUFFER";

        // Renderer-owned textures
        inline constexpr std::string_view RHI_COLOR_BUFFER   = "RHI_COLOR_BUFFER";
        inline constexpr std::string_view RHI_DEPTH_BUFFER   = "RHI_DEPTH_BUFFER";
        inline constexpr std::string_view RHI_SHADOW_MAP     = "RHI_SHADOW_MAP";
        inline constexpr std::string_view RHI_SHADOW_MASK    = "RHI_SHADOW_MASK";
        inline constexpr std::string_view SCENE_COLOR_BUFFER = "SCENE_COLOR_BUFFER";

        // Mesh GPU buffers (created/updated by MeshSync)
        inline constexpr std::string_view MESH_POSITIONS  = "MESH_POSITIONS";
        inline constexpr std::string_view MESH_TEX_COORDS = "MESH_TEX_COORDS";
        inline constexpr std::string_view MESH_NORMALS    = "MESH_NORMALS";
        inline constexpr std::string_view MESH_INDICES    = "MESH_INDICES";
    }
}

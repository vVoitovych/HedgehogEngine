#pragma once

#include <cstdint>
#include <vector>

// The game UI as the renderer will draw it: plain data, beside RenderScene, so the renderer reads
// UI without depending on the library that builds it (HedgehogUI). Coordinates are pixels of the
// view's target, origin at the top-left corner, y down.
namespace HX
{
    // A Texture value meaning "no texture": the quad is its vertex colour alone.
    inline constexpr uint32_t UI_NO_TEXTURE = UINT32_MAX;

    struct UiRect
    {
        float X      = 0.0f;
        float Y      = 0.0f;
        float Width  = 0.0f;
        float Height = 0.0f;

        bool operator==(const UiRect&) const = default;
    };

    struct UiVertex
    {
        float    Position[2] = {};
        float    Uv[2]       = {};
        uint32_t Color       = 0xFFFFFFFF; // RGBA8, red in the lowest byte (an R8G8B8A8 unorm attribute)
    };

    // One draw: IndexCount indices from FirstIndex, each relative to VertexOffset (indices are 16-bit,
    // so a list longer than 65536 vertices spans several commands), clipped to Scissor.
    struct UiDrawCommand
    {
        uint32_t Texture      = UI_NO_TEXTURE; // an index the extractor assigns, or UI_NO_TEXTURE
        UiRect   Scissor;
        uint32_t VertexOffset = 0;
        uint32_t FirstIndex   = 0;
        uint32_t IndexCount   = 0;
    };

    struct UiDrawList
    {
        std::vector<UiVertex>      Vertices;
        std::vector<uint16_t>      Indices;
        std::vector<UiDrawCommand> Commands;

        // Empties the list but keeps its storage, so rebuilding a list of the same size allocates nothing.
        void Clear()
        {
            Vertices.clear();
            Indices.clear();
            Commands.clear();
        }
    };
}

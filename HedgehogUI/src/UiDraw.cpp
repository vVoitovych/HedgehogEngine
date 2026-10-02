#include "HedgehogUI/api/UiDraw.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace HUI
{
    namespace
    {
        constexpr uint32_t MAX_COMMAND_VERTICES = std::numeric_limits<uint16_t>::max() + 1u;
        constexpr uint32_t QUAD_VERTICES        = 4;
        constexpr uint16_t QUAD_INDICES[6]      = { 0, 1, 2, 0, 2, 3 };

        uint32_t PackChannel(float value)
        {
            return static_cast<uint32_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
        }

        bool Overlaps(const HX::UiRect& a, const HX::UiRect& b)
        {
            return a.X < b.X + b.Width && b.X < a.X + a.Width && a.Y < b.Y + b.Height && b.Y < a.Y + a.Height;
        }

        // Whether the quad can join the last command: same texture and scissor, and its four
        // vertices still indexable from that command's VertexOffset.
        bool JoinsLast(const HX::UiDrawList& list, uint32_t texture, const HX::UiRect& scissor)
        {
            if (list.Commands.empty())
                return false;
            const HX::UiDrawCommand& last = list.Commands.back();
            const size_t             used = list.Vertices.size() - last.VertexOffset;
            return last.Texture == texture && last.Scissor == scissor && used + QUAD_VERTICES <= MAX_COMMAND_VERTICES;
        }
    }

    uint32_t PackColor(float r, float g, float b, float a)
    {
        return PackChannel(r) | (PackChannel(g) << 8) | (PackChannel(b) << 16) | (PackChannel(a) << 24);
    }

    void AppendQuad(HX::UiDrawList& list, const UiQuad& quad, const HX::UiRect& scissor)
    {
        const HX::UiRect& rect = quad.Rect;
        if (!(rect.Width > 0.0f && rect.Height > 0.0f) || !Overlaps(rect, scissor))
            return;

        if (!JoinsLast(list, quad.Texture, scissor))
        {
            HX::UiDrawCommand command;
            command.Texture      = quad.Texture;
            command.Scissor      = scissor;
            command.VertexOffset = static_cast<uint32_t>(list.Vertices.size());
            command.FirstIndex   = static_cast<uint32_t>(list.Indices.size());
            list.Commands.push_back(command);
        }
        HX::UiDrawCommand& command = list.Commands.back();

        // Top-left, top-right, bottom-right, bottom-left; two triangles wound the same way.
        const float      left = rect.X, top = rect.Y, right = rect.X + rect.Width, bottom = rect.Y + rect.Height;
        const HX::UiRect& uv  = quad.Uv;
        const uint16_t   base = static_cast<uint16_t>(list.Vertices.size() - command.VertexOffset);
        list.Vertices.push_back({ { left, top },     { uv.X, uv.Y },                         quad.Color });
        list.Vertices.push_back({ { right, top },    { uv.X + uv.Width, uv.Y },              quad.Color });
        list.Vertices.push_back({ { right, bottom }, { uv.X + uv.Width, uv.Y + uv.Height },  quad.Color });
        list.Vertices.push_back({ { left, bottom },  { uv.X, uv.Y + uv.Height },             quad.Color });
        for (const uint16_t corner : QUAD_INDICES)
            list.Indices.push_back(static_cast<uint16_t>(base + corner));
        command.IndexCount += 6;
    }
}

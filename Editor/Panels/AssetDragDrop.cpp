#include "AssetDragDrop.hpp"

#include <algorithm>
#include <cstring>

namespace Editor
{
    namespace
    {
        constexpr const char* ASSET_PAYLOAD = "HH_ASSET";
        constexpr float       PREVIEW_ICON  = 28.0f;
        constexpr float       TARGET_BORDER = 2.0f;

        // ImGui copies the payload as plain bytes, so it holds the path inline.
        struct AssetPayload
        {
            char        VirtualPath[512] = {};
            ContentType Type             = ContentType::Other;
        };

        ImU32 IconColor(ContentType type)
        {
            switch (type)
            {
            case ContentType::Folder:            return IM_COL32(214, 170, 72, 255);
            case ContentType::Scene:             return IM_COL32(86, 156, 214, 255);
            case ContentType::Material:          return IM_COL32(197, 108, 192, 255);
            case ContentType::Texture:           return IM_COL32(96, 180, 110, 255);
            case ContentType::Mesh:              return IM_COL32(220, 130, 70, 255);
            case ContentType::Script:            return IM_COL32(120, 120, 220, 255);
            case ContentType::Shader:
            case ContentType::Pipeline:
            case ContentType::VertexDescription: return IM_COL32(70, 170, 170, 255);
            case ContentType::RenderGraph:       return IM_COL32(200, 90, 90, 255);
            case ContentType::Other:             return IM_COL32(120, 120, 120, 255);
            }
            return IM_COL32(120, 120, 120, 255);
        }
    }

    void DrawAssetIcon(ImDrawList& drawList, const ImVec2& min, float size, ContentType type, void* icon)
    {
        if (icon)
        {
            drawList.AddImage(icon, min, ImVec2(min.x + size, min.y + size));
            return;
        }
        drawList.AddRectFilled(min, ImVec2(min.x + size, min.y + size), IconColor(type), size * 0.1f);
        const char*  glyph     = GetContentTypeGlyph(type);
        const ImVec2 glyphSize = ImGui::CalcTextSize(glyph);
        drawList.AddText(ImVec2(min.x + (size - glyphSize.x) * 0.5f, min.y + (size - glyphSize.y) * 0.5f),
                         IM_COL32(20, 20, 20, 255), glyph);
    }

    void DragAssetSource(const std::string& virtualPath, ContentType type, const std::string& name, void* icon)
    {
        if (!ImGui::BeginDragDropSource())
            return;
        AssetPayload payload;
        strncpy_s(payload.VirtualPath, virtualPath.c_str(), sizeof(payload.VirtualPath) - 1);
        payload.Type = type;
        ImGui::SetDragDropPayload(ASSET_PAYLOAD, &payload, sizeof(payload));

        const ImVec2 preview = ImGui::GetCursorScreenPos();
        DrawAssetIcon(*ImGui::GetWindowDrawList(), preview, PREVIEW_ICON, type, icon);
        ImGui::Dummy(ImVec2(PREVIEW_ICON, PREVIEW_ICON));
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(name.c_str());
        ImGui::EndDragDropSource();
    }

    std::optional<ContentOpenRequest> AcceptAssetDrop(std::initializer_list<ContentType> accepted)
    {
        return AcceptAssetDrop(std::span<const ContentType>(accepted.begin(), accepted.size()));
    }

    std::optional<ContentOpenRequest> AcceptAssetDrop(std::span<const ContentType> accepted)
    {
        if (!ImGui::BeginDragDropTarget())
            return std::nullopt;

        std::optional<ContentOpenRequest> dropped;
        const ImGuiDragDropFlags flags = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ASSET_PAYLOAD, flags))
        {
            const auto& asset = *static_cast<const AssetPayload*>(payload->Data);
            if (std::ranges::find(accepted, asset.Type) != accepted.end())
            {
                ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                                    ImGui::GetColorU32(ImGuiCol_DragDropTarget), 0.0f, 0, TARGET_BORDER);
                if (payload->IsDelivery())
                    dropped = ContentOpenRequest{ asset.VirtualPath, asset.Type };
            }
            else
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_NotAllowed);
            }
        }
        ImGui::EndDragDropTarget();
        return dropped;
    }
}

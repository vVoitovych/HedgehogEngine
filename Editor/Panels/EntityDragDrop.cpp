#include "EntityDragDrop.hpp"

#include "imgui.h"

namespace Editor
{
    namespace
    {
        constexpr const char* ENTITY_PAYLOAD = "HH_ENTITY";
        constexpr float       TARGET_BORDER  = 2.0f;
    }

    void DragEntitySource(ECS::Entity entity, const std::string& name)
    {
        if (!ImGui::BeginDragDropSource())
            return;
        ImGui::SetDragDropPayload(ENTITY_PAYLOAD, &entity, sizeof(entity));
        ImGui::TextUnformatted(name.c_str());
        ImGui::EndDragDropSource();
    }

    std::optional<ECS::Entity> AcceptEntityDrop()
    {
        if (!ImGui::BeginDragDropTarget())
            return std::nullopt;

        std::optional<ECS::Entity> dropped;
        const ImGuiDragDropFlags flags = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ENTITY_PAYLOAD, flags))
        {
            ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                                ImGui::GetColorU32(ImGuiCol_DragDropTarget), 0.0f, 0, TARGET_BORDER);
            if (payload->IsDelivery())
                dropped = *static_cast<const ECS::Entity*>(payload->Data);
        }
        ImGui::EndDragDropTarget();
        return dropped;
    }
}

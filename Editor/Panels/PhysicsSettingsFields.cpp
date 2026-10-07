#include "PhysicsSettingsFields.hpp"

#include "HedgehogEngine/HedgehogSettings/api/PhysicsSettings.hpp"

#include "imgui.h"

#include <string>

namespace Editor
{
    void DrawPhysicsSettingsFields(HedgehogSettings::PhysicsSettings& physics)
    {
        using HedgehogSettings::PhysicsSettings;

        ImGui::DragFloat3("Gravity", physics.Gravity.data(), 0.1f);
        ImGui::SetItemTooltip("Metres per second squared. The engine is Z-up.");

        ImGui::InputInt("Worker threads", &physics.WorkerThreads);
        if (physics.WorkerThreads < PhysicsSettings::AUTO_WORKER_THREADS)
            physics.WorkerThreads = PhysicsSettings::AUTO_WORKER_THREADS;
        ImGui::SetItemTooltip("-1: the hardware's threads minus one. 0: on the thread that steps.");

        if (ImGui::TreeNode("Physics layers"))
        {
            for (uint32_t layer = 0; layer < PhysicsSettings::LAYER_COUNT; ++layer)
            {
                std::string& name = physics.LayerNames[layer];
                char         buffer[64];
                buffer[name.copy(buffer, sizeof(buffer) - 1)] = '\0';

                ImGui::PushID(static_cast<int>(layer));
                if (ImGui::InputText(("Layer " + std::to_string(layer)).c_str(), buffer, sizeof(buffer)))
                    name = buffer;
                ImGui::PopID();
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Collision matrix"))
        {
            constexpr int columns = static_cast<int>(PhysicsSettings::LAYER_COUNT) + 1;
            if (ImGui::BeginTable("##collisions", columns, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX))
            {
                ImGui::TableSetupColumn("Layer");
                for (uint32_t column = 0; column < PhysicsSettings::LAYER_COUNT; ++column)
                    ImGui::TableSetupColumn(std::to_string(column).c_str());
                ImGui::TableHeadersRow();

                for (uint32_t row = 0; row < PhysicsSettings::LAYER_COUNT; ++row)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(physics.GetLayerDisplayName(row).c_str());
                    for (uint32_t column = 0; column <= row; ++column)
                    {
                        ImGui::TableNextColumn();
                        ImGui::PushID(static_cast<int>(row * PhysicsSettings::LAYER_COUNT + column));
                        bool collides = physics.Collides(row, column);
                        if (ImGui::Checkbox("##c", &collides))
                            physics.SetCollides(row, column, collides);
                        ImGui::SetItemTooltip("%s and %s", physics.GetLayerDisplayName(row).c_str(),
                                              physics.GetLayerDisplayName(column).c_str());
                        ImGui::PopID();
                    }
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }
}

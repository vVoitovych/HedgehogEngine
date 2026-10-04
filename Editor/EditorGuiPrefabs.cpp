#include "EditorGui.hpp"
#include "EditorTheme.hpp"

#include "HedgehogEngine/api/ECS/components/PrefabInstanceComponent.hpp"
#include "HedgehogEngine/api/Engine.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Prefab/PrefabManager.hpp"

#include "ECS/api/ECS.hpp"

#include "imgui.h"

#include <filesystem>
#include <string>

namespace Editor
{
    namespace
    {
        // "LightComponent.LightIntensity", or "LightComponent (added)" for a whole component.
        std::string Describe(const EcsSerialization::PropertyOverride& entry)
        {
            return entry.Property.empty() ? entry.Component + " (added)" : entry.Component + "." + entry.Property;
        }
    }

    void EditorGui::RefreshPrefabOverrides(HedgehogEngine::Engine& context)
    {
        auto& engineContext = context.GetEngineContext();
        if (m_PrefabOverrides.Entity != m_SelectedEntity || engineContext.GetPlayState() != HedgehogEngine::PlayState::Edit)
            m_PrefabOverrides.Stale = true;
        if (!m_PrefabOverrides.Stale)
            return;

        HedgehogEngine::PrefabManager& prefabs = engineContext.GetPrefabs();
        auto&                          ecs     = engineContext.GetECS();
        m_PrefabOverrides.Entity       = m_SelectedEntity;
        m_PrefabOverrides.InstanceRoot = m_SelectedEntity ? prefabs.GetInstanceRoot(*m_SelectedEntity) : ECS::INVALID_ENTITY;
        m_PrefabOverrides.Instance.clear();
        m_PrefabMarks.Overridden.clear();
        m_PrefabOverrides.Stale = false;
        if (m_PrefabOverrides.InstanceRoot == ECS::INVALID_ENTITY)
            return;

        m_PrefabOverrides.LocalId  = ecs.GetComponent<HedgehogEngine::PrefabInstanceComponent>(*m_SelectedEntity).LocalId;
        m_PrefabOverrides.Instance = prefabs.GetOverrides(*m_SelectedEntity);
        for (const EcsSerialization::PropertyOverride& entry : m_PrefabOverrides.Instance)
        {
            if (entry.LocalId == m_PrefabOverrides.LocalId && !entry.Property.empty())
                m_PrefabMarks.Overridden.emplace_back(entry.Component, entry.Property);
        }
    }

    void EditorGui::ChangePrefabOverrides(HedgehogEngine::Engine& context, const EcsSerialization::OverrideSet& overrides, bool apply)
    {
        if (!m_SelectedEntity || overrides.empty())
            return;
        HedgehogEngine::PrefabManager& prefabs = context.GetEngineContext().GetPrefabs();
        if (apply)
            (void)prefabs.Apply(*m_SelectedEntity, overrides);
        else
            (void)prefabs.Revert(*m_SelectedEntity, overrides);
        m_PrefabOverrides.Stale = true;
    }

    void EditorGui::HandlePrefabRowRequest(HedgehogEngine::Engine& context)
    {
        if (!m_PrefabMarks.Requested)
            return;
        const Reflection::PrefabOverrideMarks::Request request = *m_PrefabMarks.Requested;
        m_PrefabMarks.Requested.reset();

        EcsSerialization::OverrideSet picked;
        for (const EcsSerialization::PropertyOverride& entry : m_PrefabOverrides.Instance)
        {
            if (entry.LocalId == m_PrefabOverrides.LocalId && entry.Component == request.Component &&
                entry.Property == request.Property)
                picked.push_back(entry);
        }
        ChangePrefabOverrides(context, picked, request.Apply);
    }

    void EditorGui::DrawPrefabBar(HedgehogEngine::Engine& context)
    {
        if (m_PrefabOverrides.InstanceRoot == ECS::INVALID_ENTITY)
            return;
        auto&             engineContext = context.GetEngineContext();
        auto&             ecs           = engineContext.GetECS();
        const std::string path =
            ecs.GetComponent<HedgehogEngine::PrefabInstanceComponent>(m_PrefabOverrides.InstanceRoot).PrefabPath;
        // Applying writes the prefab file, so it waits for Edit, where values are not a game's.
        const bool editing = engineContext.GetPlayState() == HedgehogEngine::PlayState::Edit;
        const bool any     = !m_PrefabOverrides.Instance.empty();

        ImGui::Spacing();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(Theme::Resolve(Theme::PREFAB_TINT), "Prefab");
        ImGui::SameLine();
        ImGui::TextUnformatted(std::filesystem::path(path).stem().string().c_str());
        ImGui::SetItemTooltip("%s", path.c_str());

        if (ImGui::SmallButton("Select"))
            m_ContentPanel->Reveal(path);
        ImGui::SetItemTooltip("Show the prefab in the Project panel.");
        ImGui::SameLine();
        if (!any) ImGui::BeginDisabled();
        if (ImGui::SmallButton("Revert All"))
            ChangePrefabOverrides(context, m_PrefabOverrides.Instance, false);
        ImGui::SetItemTooltip("Put the prefab's values back on every entity of this instance.");
        ImGui::SameLine();
        if (!editing) ImGui::BeginDisabled();
        if (ImGui::SmallButton("Apply All"))
            ChangePrefabOverrides(context, m_PrefabOverrides.Instance, true);
        ImGui::SetItemTooltip("Write this instance's values into the prefab and every other instance.");
        if (!editing) ImGui::EndDisabled();
        if (!any) ImGui::EndDisabled();

        // The selected entity's own overrides, each with its own Revert and Apply, which also reach
        // the fields the inspector draws by hand.
        size_t own = 0;
        for (const EcsSerialization::PropertyOverride& entry : m_PrefabOverrides.Instance)
            own += entry.LocalId == m_PrefabOverrides.LocalId ? 1 : 0;
        if (own > 0)
        {
            const std::string label = "Overrides (" + std::to_string(own) + ")###PrefabOverrides";
            if (ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
            {
                std::optional<std::pair<EcsSerialization::PropertyOverride, bool>> picked;
                for (const EcsSerialization::PropertyOverride& entry : m_PrefabOverrides.Instance)
                {
                    if (entry.LocalId != m_PrefabOverrides.LocalId)
                        continue;
                    const std::string name = Describe(entry);
                    ImGui::PushID(name.c_str());
                    if (ImGui::SmallButton("Revert"))
                        picked.emplace(entry, false);
                    ImGui::SameLine();
                    if (!editing) ImGui::BeginDisabled();
                    if (ImGui::SmallButton("Apply"))
                        picked.emplace(entry, true);
                    if (!editing) ImGui::EndDisabled();
                    ImGui::SameLine();
                    ImGui::TextColored(Theme::Resolve(Theme::PREFAB_TINT), "%s", name.c_str());
                    ImGui::PopID();
                }
                ImGui::TreePop();
                if (picked)
                    ChangePrefabOverrides(context, { picked->first }, picked->second);
            }
        }
        ImGui::Separator();
    }
}

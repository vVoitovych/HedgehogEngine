#include "AnimatorControllerEditorWindow.hpp"

#include "ControllerEditing.hpp"
#include "Tools/RenderGraphEditor/RenderGraphEditorStyle.hpp"
#include "Widgets/PropertyFields.hpp"

#include "imgui.h"

#include <array>
#include <cmath>

namespace Editor
{
    namespace
    {
        using HedgehogAnimation::AnimatorParameterType;
        using HedgehogAnimation::ConditionOp;

        constexpr std::array<AnimatorParameterType, 4> PARAMETER_TYPES = {
            AnimatorParameterType::Float, AnimatorParameterType::Int, AnimatorParameterType::Bool, AnimatorParameterType::Trigger
        };
        constexpr std::array<ConditionOp, 7> CONDITION_OPS = {
            ConditionOp::Greater, ConditionOp::Less, ConditionOp::Equals, ConditionOp::NotEquals,
            ConditionOp::True, ConditionOp::False, ConditionOp::Triggered
        };

        // A text field that applies on Enter, as the render graph editor's: ImGui keeps its own
        // copy while it is typed in, so refilling the buffer every frame loses nothing.
        bool InputTextOnEnter(const char* label, const std::string& value, std::string& edited)
        {
            char buffer[256];
            buffer[value.copy(buffer, sizeof(buffer) - 1)] = '\0';
            if (!ImGui::InputText(label, buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
                return false;
            edited = buffer;
            return true;
        }

        bool HasValue(ConditionOp op)
        {
            return op != ConditionOp::True && op != ConditionOp::False && op != ConditionOp::Triggered;
        }

        // A parameter's default, or a condition's value: a whole number for an Int, a checkbox for a
        // Bool, nothing for a Trigger.
        bool ValueField(const char* id, AnimatorParameterType type, float& value)
        {
            switch (type)
            {
            case AnimatorParameterType::Float:
                return ImGui::DragFloat(id, &value, 0.05f);
            case AnimatorParameterType::Int:
            {
                int whole = static_cast<int>(std::lround(value));
                if (!ImGui::DragInt(id, &whole))
                    return false;
                value = static_cast<float>(whole);
                return true;
            }
            case AnimatorParameterType::Bool:
            {
                bool on = value != 0.0f;
                if (!ImGui::Checkbox(id, &on))
                    return false;
                value = on ? 1.0f : 0.0f;
                return true;
            }
            case AnimatorParameterType::Trigger:
                ImGui::TextDisabled("-");
                return false;
            }
            return false;
        }

        void DrawProblemText(const std::unordered_map<uint32_t, std::string>& problems, uint32_t index)
        {
            if (const auto it = problems.find(index); it != problems.end())
            {
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextColored(PROBLEM_COLOR, "%s", it->second.c_str());
                ImGui::PopTextWrapPos();
            }
        }
    }

    void AnimatorControllerEditorWindow::DrawDetails(const AnimatorPreview& preview)
    {
        if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen))
            DrawParameters();
        ImGui::Separator();
        if (m_SelectedState)
            DrawStateDetails(*m_SelectedState, preview);
        else if (m_SelectedTransition)
            DrawTransitionDetails(*m_SelectedTransition);
        else
            ImGui::TextDisabled("Select a state or a transition to edit it.");
    }

    void AnimatorControllerEditorWindow::DrawParameters()
    {
        std::optional<uint32_t> deleted;
        for (uint32_t p = 0; p < m_Edited.Parameters.size(); ++p)
        {
            HedgehogAnimation::AnimatorParameter& parameter = m_Edited.Parameters[p];
            ImGui::PushID(static_cast<int>(p));
            std::string name;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.4f);
            if (InputTextOnEnter("##name", parameter.Name, name))
            {
                if (RenameParameter(m_Edited, p, name))
                    ApplyEdit(false);
                else
                    m_Status = "A parameter needs a name no other parameter has.";
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70.0f);
            if (ImGui::BeginCombo("##type", HedgehogAnimation::GetParameterTypeName(parameter.Type)))
            {
                for (const AnimatorParameterType type : PARAMETER_TYPES)
                {
                    if (ImGui::Selectable(HedgehogAnimation::GetParameterTypeName(type), type == parameter.Type)
                        && SetParameterType(m_Edited, p, type))
                    {
                        ApplyEdit(false);
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x);
            if (ValueField("##default", parameter.Type, parameter.Default))
                ApplyEdit(false);
            ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight());
            if (ImGui::Button("X", ImVec2(ImGui::GetFrameHeight(), 0.0f)))
                deleted = p;
            ImGui::SetItemTooltip("Delete the parameter and the conditions testing it");
            ImGui::PopID();
        }
        if (deleted && DeleteParameter(m_Edited, *deleted))
            ApplyEdit(false);

        if (ImGui::Button("Add parameter"))
            ImGui::OpenPopup("AddParameter");
        if (ImGui::BeginPopup("AddParameter"))
        {
            for (const AnimatorParameterType type : PARAMETER_TYPES)
            {
                const char* typeName = HedgehogAnimation::GetParameterTypeName(type);
                if (ImGui::MenuItem(typeName))
                {
                    AddParameter(m_Edited, typeName, type);
                    ApplyEdit(false);
                }
            }
            ImGui::EndPopup();
        }
    }

    void AnimatorControllerEditorWindow::DrawStateDetails(uint32_t state, const AnimatorPreview& preview)
    {
        HedgehogAnimation::AnimatorState& data = m_Edited.States[state];
        ImGui::Text("State");
        DrawProblemText(m_StateProblems, state);
        if (BeginPropertyTable("##controllerState"))
        {
            std::string edited;
            PropertyLabel("Name");
            if (InputTextOnEnter("##name", data.Name, edited))
            {
                const std::string old = data.Name;
                if (RenameState(m_Edited, state, edited))
                {
                    RenameStateNode(m_Layout, old, edited);
                    ApplyEdit(false);
                }
                else
                {
                    m_Status = "A state needs a name no other state has.";
                }
            }

            // The clip by name, or picked from the selected mesh's clips.
            PropertyLabel("Clip");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x);
            if (InputTextOnEnter("##clip", data.Clip, edited))
            {
                data.Clip = edited;
                ApplyEdit(false);
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(preview.Clips.empty());
            if (ImGui::BeginCombo("##clips", nullptr, ImGuiComboFlags_NoPreview))
            {
                for (const std::string& clip : preview.Clips)
                {
                    if (ImGui::Selectable(clip.c_str(), clip == data.Clip))
                    {
                        data.Clip = clip;
                        ApplyEdit(false);
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::EndDisabled();
            ImGui::SetItemTooltip(preview.Clips.empty() ? "Select an entity with a skinned mesh to pick from its clips"
                                                        : "The selected entity's mesh's clips");

            PropertyLabel("Speed");
            if (ImGui::DragFloat("##speed", &data.Speed, 0.01f))
                ApplyEdit(false);
            PropertyLabel("Loop");
            if (ImGui::Checkbox("##loop", &data.Loop))
                ApplyEdit(false);

            PropertyLabel("Speed parameter");
            if (ImGui::BeginCombo("##speedParameter", data.SpeedParameter.empty() ? "(none)" : data.SpeedParameter.c_str()))
            {
                if (ImGui::Selectable("(none)", data.SpeedParameter.empty()))
                {
                    data.SpeedParameter.clear();
                    ApplyEdit(false);
                }
                for (const HedgehogAnimation::AnimatorParameter& parameter : m_Edited.Parameters)
                {
                    if (parameter.Type == AnimatorParameterType::Float
                        && ImGui::Selectable(parameter.Name.c_str(), parameter.Name == data.SpeedParameter))
                    {
                        data.SpeedParameter = parameter.Name;
                        ApplyEdit(false);
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SetItemTooltip("A Float parameter the speed is multiplied by");
            EndPropertyTable();
        }

        if (state == m_Edited.DefaultState)
            ImGui::TextDisabled("The default state: the controller starts here.");
        else if (ImGui::Button("Make default"))
        {
            SetDefaultState(m_Edited, state);
            ApplyEdit(false);
        }
        if (ImGui::Button("Delete state"))
        {
            const std::string name = data.Name;
            DeleteState(m_Edited, state);
            RemoveStateNode(m_Layout, name);
            ApplyEdit(true);
        }
    }

    void AnimatorControllerEditorWindow::DrawTransitionDetails(uint32_t transition)
    {
        HedgehogAnimation::AnimatorTransition& data = m_Edited.Transitions[transition];
        const auto stateName = [this](uint32_t state) -> const char*
        {
            if (state == HedgehogAnimation::ANY_STATE)
                return "Any State";
            return state < m_Edited.States.size() ? m_Edited.States[state].Name.c_str() : "?";
        };
        ImGui::Text("Transition: %s -> %s", stateName(data.From), stateName(data.To));
        DrawProblemText(m_TransitionProblems, transition);

        // The list is the order transitions are checked in, Any State's before a state's own.
        const uint32_t count = static_cast<uint32_t>(m_Edited.Transitions.size());
        ImGui::TextDisabled("Checked %u of %u", transition + 1, count);
        ImGui::SameLine();
        std::optional<uint32_t> place;
        ImGui::BeginDisabled(transition == 0);
        if (ImGui::ArrowButton("##earlier", ImGuiDir_Up))
            place = transition - 1;
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(transition + 1 >= count);
        if (ImGui::ArrowButton("##later", ImGuiDir_Down))
            place = transition + 1;
        ImGui::EndDisabled();
        if (place && MoveTransition(m_Edited, transition, *place))
        {
            ApplyEdit(true); // the links are renumbered
            m_FocusTransition = *place;
            return;
        }

        if (BeginPropertyTable("##controllerTransition"))
        {
            PropertyLabel("Has exit time");
            if (ImGui::Checkbox("##hasExitTime", &data.HasExitTime))
                ApplyEdit(false);
            PropertyLabel("Exit time");
            ImGui::BeginDisabled(!data.HasExitTime);
            if (ImGui::DragFloat("##exitTime", &data.ExitTime, 0.01f, 0.0f, 100.0f))
                ApplyEdit(false);
            ImGui::EndDisabled();
            ImGui::SetItemTooltip("In lengths of the state's clip since it began");
            PropertyLabel("Duration");
            if (ImGui::DragFloat("##duration", &data.Duration, 0.01f, 0.0f, 10.0f, "%.2f s"))
                ApplyEdit(false);
            if (data.From == HedgehogAnimation::ANY_STATE)
            {
                PropertyLabel("Can transition to self");
                if (ImGui::Checkbox("##toSelf", &data.CanTransitionToSelf))
                    ApplyEdit(false);
            }
            EndPropertyTable();
        }

        ImGui::Text("Conditions");
        std::optional<uint32_t> removed;
        for (uint32_t c = 0; c < data.Conditions.size(); ++c)
        {
            HedgehogAnimation::TransitionCondition& condition = data.Conditions[c];
            ImGui::PushID(static_cast<int>(c));
            const bool known = condition.Parameter < m_Edited.Parameters.size();
            const AnimatorParameterType type = known ? m_Edited.Parameters[condition.Parameter].Type : AnimatorParameterType::Float;

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.38f);
            if (ImGui::BeginCombo("##parameter", known ? m_Edited.Parameters[condition.Parameter].Name.c_str() : "?"))
            {
                for (uint32_t p = 0; p < m_Edited.Parameters.size(); ++p)
                {
                    if (ImGui::Selectable(m_Edited.Parameters[p].Name.c_str(), p == condition.Parameter)
                        && SetConditionParameter(m_Edited, transition, c, p))
                    {
                        ApplyEdit(false);
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::BeginCombo("##op", HedgehogAnimation::GetConditionOpName(condition.Op)))
            {
                for (const ConditionOp op : CONDITION_OPS)
                {
                    if (HedgehogAnimation::IsConditionOpValid(type, op)
                        && ImGui::Selectable(HedgehogAnimation::GetConditionOpName(op), op == condition.Op))
                    {
                        condition.Op = op;
                        ApplyEdit(false);
                    }
                }
                ImGui::EndCombo();
            }
            if (HasValue(condition.Op))
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x);
                if (ValueField("##value", type, condition.Value))
                    ApplyEdit(false);
            }
            ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight());
            if (ImGui::Button("X", ImVec2(ImGui::GetFrameHeight(), 0.0f)))
                removed = c;
            ImGui::PopID();
        }
        if (removed && DeleteCondition(m_Edited, transition, *removed))
            ApplyEdit(false);

        ImGui::BeginDisabled(m_Edited.Parameters.empty());
        if (ImGui::Button("Add condition") && AddCondition(m_Edited, transition, 0))
            ApplyEdit(false);
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Delete transition"))
        {
            DeleteTransition(m_Edited, transition);
            ApplyEdit(true);
        }
    }
}

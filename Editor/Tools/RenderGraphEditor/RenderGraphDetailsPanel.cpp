// The render graph editor's Details panel: edits the one selected node of the open graph.

#include "RenderGraphEditorWindow.hpp"

#include "GraphEditing.hpp"
#include "RenderGraphEditorStyle.hpp"

#include "HedgehogRenderer/Graph/GraphAssetVocabulary.hpp"
#include "HedgehogRenderer/Renderer.hpp"

#include "imgui.h"
#include "imgui_node_editor.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace ed = ax::NodeEditor;

namespace Editor
{
    namespace
    {
        constexpr const char* NO_VALUE = "(default)";

        constexpr std::array<const char*, 3> SIZE_POLICY_KINDS = { "Absolute", "RelativeToResult", "RelativeToSwapchain" };

        // A text field that applies on Enter. The buffer is refilled from value every frame; while
        // the field is being typed in, ImGui keeps its own copy, so nothing is lost.
        bool InputTextOnEnter(const char* label, const std::string& value, std::string& edited)
        {
            char buffer[256];
            buffer[value.copy(buffer, sizeof(buffer) - 1)] = '\0';
            if (!ImGui::InputText(label, buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
                return false;
            edited = buffer;
            return true;
        }

        // A combo of every vocabulary format. With allowNone, a "(default)" entry comes first and
        // choosing it gives std::nullopt.
        bool FormatCombo(const char* label, std::optional<RHI::Format> current, bool allowNone,
                         std::optional<RHI::Format>& chosen)
        {
            const std::optional<std::string_view> name = current ? Renderer::GetFormatName(*current) : std::nullopt;
            const std::string preview = name ? std::string(*name) : std::string(current ? "Undefined" : NO_VALUE);
            bool changed = false;
            if (ImGui::BeginCombo(label, preview.c_str()))
            {
                if (allowNone && ImGui::Selectable(NO_VALUE, !current))
                {
                    chosen  = std::nullopt;
                    changed = true;
                }
                for (const Renderer::FormatVocabularyEntry& entry : Renderer::GetFormatVocabulary())
                {
                    if (ImGui::Selectable(std::string(entry.Name).c_str(), current == entry.Value))
                    {
                        chosen  = entry.Value;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            return changed;
        }

        bool SizePolicyEditor(Renderer::RGSizePolicy& size)
        {
            bool changed = false;
            int  kind    = static_cast<int>(size.Kind);
            if (ImGui::Combo("Size", &kind, SIZE_POLICY_KINDS.data(), static_cast<int>(SIZE_POLICY_KINDS.size())))
            {
                size = kind == 0 ? Renderer::RGSizePolicy::MakeAbsolute(1024, 1024)
                     : kind == 1 ? Renderer::RGSizePolicy::MakeRelativeToResult(1.0f)
                                 : Renderer::RGSizePolicy::MakeRelativeToSwapchain(1.0f);
                changed = true;
            }
            if (size.Kind == Renderer::RGSizePolicyKind::Absolute)
            {
                int extent[2] = { static_cast<int>(size.Width), static_cast<int>(size.Height) };
                if (ImGui::InputInt2("Width, height", extent))
                {
                    size.Width  = static_cast<uint32_t>(std::max(1, extent[0]));
                    size.Height = static_cast<uint32_t>(std::max(1, extent[1]));
                    changed     = true;
                }
            }
            else
            {
                float scale = size.Scale;
                if (ImGui::InputFloat("Scale", &scale, 0.25f, 1.0f, "%.3f") && scale > 0.0f)
                {
                    size.Scale = scale;
                    changed    = true;
                }
            }
            return changed;
        }

        const char* KindName(GraphNodeKind kind)
        {
            switch (kind)
            {
            case GraphNodeKind::Resource: return "Resource";
            case GraphNodeKind::Import:   return "Import";
            case GraphNodeKind::Output:   return "Output";
            case GraphNodeKind::Pass:     return "Pass";
            }
            return "";
        }
    }

    void RenderGraphEditorWindow::DrawDetails(const Renderer::Renderer& renderer)
    {
        ImGui::SeparatorText("Details");
        if (!m_HasGraph)
        {
            ImGui::TextDisabled("Open a graph.");
            return;
        }

        ed::SetCurrentEditor(m_Canvas);
        ed::NodeId selected;
        const bool single = ed::GetSelectedObjectCount() == 1 && ed::GetSelectedNodes(&selected, 1) == 1;
        ed::SetCurrentEditor(nullptr);

        const GraphCanvasNode* found = single ? FindNode(static_cast<uint32_t>(selected.Get())) : nullptr;
        if (!found)
        {
            ImGui::TextWrapped("Select one node to edit it. Right-click the canvas to add a pass, resource, "
                               "import or output; drag between a pin and a pass's slot to bind it.");
            return;
        }
        // A copy: every edit below rebuilds m_Model, which would leave a reference into it dangling.
        // The copy's kind and index stay right for the rest of the frame, because only removing a
        // node shifts indices, and that is the last thing done here.
        const GraphCanvasNode node = *found;

        ImGui::TextColored(node.Problem ? PROBLEM_COLOR : RESOURCE_COLOR, "%s", KindName(node.Kind));
        DrawNameField(node);
        if (node.Kind == GraphNodeKind::Pass)
            DrawPassDetails(renderer, node);
        else
            DrawResourceDetails(node);

        ImGui::Spacing();
        if (ImGui::Button("Delete node"))
        {
            if (node.Kind == GraphNodeKind::Pass)
                GraphEdit::RemovePass(m_Edited, node.Index);
            else
                GraphEdit::RemoveResourceLike(m_Edited, node.Kind, node.Index);
            ApplyEdit(true);
        }
    }

    void RenderGraphEditorWindow::DrawNameField(const GraphCanvasNode& node)
    {
        std::string name;
        if (!InputTextOnEnter("Name", node.Name, name))
            return;

        const std::optional<std::string> error = node.Kind == GraphNodeKind::Pass
            ? GraphEdit::RenamePass(m_Edited, node.Index, name)
            : GraphEdit::RenameResourceLike(m_Edited, node.Kind, node.Index, name);
        if (error)
        {
            m_Status = *error;
            return;
        }
        // The node keeps its place under its new name.
        if (const auto it = m_Layout.find(node.Key); it != m_Layout.end())
            m_Layout[GetGraphNodeKey(node.Kind, name)] = it->second;
        m_Status.clear();
        ApplyEdit(false);
    }

    void RenderGraphEditorWindow::DrawPassDetails(const Renderer::Renderer& renderer, const GraphCanvasNode& node)
    {
        Renderer::GraphAssetPass&     pass = m_Edited.Passes[node.Index];
        const Renderer::PassTypeInfo* type = renderer.FindPassType(pass.Type);
        ImGui::Text("Type: %s", pass.Type.c_str());
        if (!type)
            ImGui::TextColored(PROBLEM_COLOR, "No pass type is registered under this name.");

        ImGui::SeparatorText("Parameters");
        bool changed = false;
        if (type)
        {
            for (const Renderer::PassParameterInfo& parameter : type->Parameters)
            {
                const auto it = std::ranges::find(pass.Parameters, parameter.Name, &Renderer::GraphAssetParameter::Name);
                const std::optional<std::string> value = it != pass.Parameters.end() ? std::optional(it->Value) : std::nullopt;
                ImGui::PushID(parameter.Name.c_str());
                switch (parameter.Kind)
                {
                case Renderer::PassParameterKind::Flag:
                {
                    const char* items[] = { NO_VALUE, "true", "false" };
                    int index = !value ? 0 : (*value == "true" ? 1 : 2);
                    if (ImGui::Combo(parameter.Name.c_str(), &index, items, 3))
                    {
                        GraphEdit::SetParameter(m_Edited, node.Index, parameter.Name,
                                                index == 0 ? std::nullopt : std::optional<std::string>(items[index]));
                        changed = true;
                    }
                    break;
                }
                case Renderer::PassParameterKind::Format:
                {
                    std::optional<RHI::Format> chosen;
                    if (FormatCombo(parameter.Name.c_str(), value ? Renderer::ResolveFormat(*value) : std::nullopt, true, chosen))
                    {
                        GraphEdit::SetParameter(m_Edited, node.Index, parameter.Name,
                                                chosen ? std::optional(std::string(*Renderer::GetFormatName(*chosen)))
                                                       : std::nullopt);
                        changed = true;
                    }
                    break;
                }
                case Renderer::PassParameterKind::SizePolicy:
                case Renderer::PassParameterKind::Text:
                {
                    // Enter applies; an empty value falls back to the pass type's default.
                    std::string edited;
                    if (InputTextOnEnter(parameter.Name.c_str(), value.value_or(""), edited))
                    {
                        const bool valid = parameter.Kind != Renderer::PassParameterKind::SizePolicy
                                        || edited.empty() || Renderer::ResolveSizePolicy(edited).has_value();
                        if (valid)
                        {
                            GraphEdit::SetParameter(m_Edited, node.Index, parameter.Name,
                                                    edited.empty() ? std::nullopt : std::optional(edited));
                            changed = true;
                        }
                        else
                        {
                            m_Status = "'" + edited + "' is not a size policy, e.g. RelativeToResult(0.5).";
                        }
                    }
                    break;
                }
                }
                ImGui::PopID();
            }
            if (type->Parameters.empty())
                ImGui::TextDisabled("This pass type has none.");
        }

        // Parameters the asset sets that the type does not declare: the runtime rejects them.
        for (const Renderer::GraphAssetParameter& parameter : pass.Parameters)
        {
            const bool declared = type && std::ranges::any_of(type->Parameters, [&](const auto& info)
            {
                return info.Name == parameter.Name;
            });
            if (declared)
                continue;
            ImGui::PushID(parameter.Name.c_str());
            ImGui::TextColored(PROBLEM_COLOR, "%s = %s (not a parameter)", parameter.Name.c_str(), parameter.Value.c_str());
            ImGui::SameLine();
            const std::string name = parameter.Name;
            const bool remove = ImGui::SmallButton("Remove");
            ImGui::PopID();
            if (remove)
            {
                GraphEdit::SetParameter(m_Edited, node.Index, name, std::nullopt);
                changed = true;
                break; // the loop's vector changed
            }
        }

        ImGui::SeparatorText("Bindings");
        for (const GraphCanvasPin& pin : node.Pins)
        {
            const auto binding = std::ranges::find(pass.Bindings, pin.Slot, &Renderer::GraphAssetBinding::Slot);
            ImGui::PushID(pin.Slot.c_str());
            if (binding == pass.Bindings.end())
            {
                ImGui::TextColored(PROBLEM_COLOR, "%s: unbound", pin.Slot.c_str());
            }
            else
            {
                ImGui::Text("%s: %s", pin.Slot.c_str(), binding->Resource.c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Unbind"))
                {
                    GraphEdit::UnbindSlot(m_Edited, node.Index, pin.Slot);
                    changed = true;
                }
            }
            ImGui::PopID();
            if (changed)
                break; // the pass's bindings changed under the loop
        }

        if (changed)
            ApplyEdit(false);
    }

    void RenderGraphEditorWindow::DrawResourceDetails(const GraphCanvasNode& node)
    {
        bool changed = false;
        std::optional<RHI::Format> format;
        switch (node.Kind)
        {
        case GraphNodeKind::Output:
        {
            Renderer::GraphAssetOutput& output = m_Edited.Outputs[node.Index];
            ImGui::Text("Output slot %u", output.Slot);
            if (FormatCombo("Format", output.Format, false, format) && format)
            {
                output.Format = *format;
                changed       = true;
            }
            changed |= SizePolicyEditor(output.Size);
            break;
        }
        case GraphNodeKind::Import:
        {
            Renderer::GraphAssetImport& import = m_Edited.Imports[node.Index];
            ImGui::TextDisabled("Supplied from outside the graph, e.g. by the shared frame phase.");
            if (FormatCombo("Format", import.Format, false, format) && format)
            {
                import.Format = *format;
                changed       = true;
            }
            break;
        }
        default:
        {
            Renderer::GraphAssetResource& resource = m_Edited.Resources[node.Index];
            if (FormatCombo("Format", resource.Format, false, format) && format)
            {
                resource.Format = *format;
                changed         = true;
            }
            changed |= SizePolicyEditor(resource.Size);
            break;
        }
        }
        if (changed)
            ApplyEdit(false);
    }
}

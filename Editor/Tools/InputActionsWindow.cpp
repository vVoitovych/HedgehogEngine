#include "InputActionsWindow.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogInput/api/DefaultInputActions.hpp"
#include "HedgehogInput/api/InputActionFile.hpp"
#include "HedgehogInput/api/InputNames.hpp"

#include "imgui.h"

#include <array>
#include <set>
#include <utility>

namespace Editor
{
    namespace
    {
        constexpr const char* PATH = HedgehogEngine::EngineContext::INPUT_ACTIONS_PATH;
        constexpr ImVec4      ERROR_COLOR(1.0f, 0.3f, 0.3f, 1.0f);

        struct SourceName
        {
            HInput::BindingSource Source;
            const char*           Name;
        };

        constexpr std::array SOURCES = {
            SourceName{ HInput::BindingSource::Key, "Key" },
            SourceName{ HInput::BindingSource::MouseButton, "Mouse button" },
            SourceName{ HInput::BindingSource::KeyAxis, "Key axis" },
            SourceName{ HInput::BindingSource::PointerDeltaX, "Pointer delta X" },
            SourceName{ HInput::BindingSource::PointerDeltaY, "Pointer delta Y" },
            SourceName{ HInput::BindingSource::ScrollX, "Scroll X" },
            SourceName{ HInput::BindingSource::ScrollY, "Scroll Y" },
            SourceName{ HInput::BindingSource::GamepadButton, "Gamepad button" },
            SourceName{ HInput::BindingSource::GamepadAxis, "Gamepad axis" },
        };

        const char* SourceLabel(HInput::BindingSource source)
        {
            for (const SourceName& entry : SOURCES)
            {
                if (entry.Source == source)
                    return entry.Name;
            }
            return "?";
        }

        // The name of code for source, or "(none)".
        std::string CodeName(HInput::BindingSource source, uint16_t code)
        {
            std::string_view name;
            switch (source)
            {
            case HInput::BindingSource::Key:
            case HInput::BindingSource::KeyAxis:       name = HInput::GetKeyName(code); break;
            case HInput::BindingSource::MouseButton:   name = HInput::GetMouseButtonName(code); break;
            case HInput::BindingSource::GamepadButton: name = HInput::GetGamepadButtonName(code); break;
            case HInput::BindingSource::GamepadAxis:   name = HInput::GetGamepadAxisName(code); break;
            default:                                   break;
            }
            return name.empty() ? std::string("(none)") : std::string(name);
        }

        // A combo over a name table, setting code to the picked entry's.
        template<typename Table, typename CodeOf>
        void NameCombo(const char* label, uint16_t& code, const std::string& current, const Table& table, CodeOf codeOf)
        {
            if (!ImGui::BeginCombo(label, current.c_str()))
                return;
            for (const auto& entry : table)
            {
                const uint16_t entryCode = codeOf(entry);
                if (ImGui::Selectable(std::string(entry.Name).c_str(), entryCode == code))
                    code = entryCode;
            }
            ImGui::EndCombo();
        }
    }

    void InputActionsWindow::OpenFile(const FS::FileSystemManager& fileSystem)
    {
        Open = true;
        Load(fileSystem);
    }

    void InputActionsWindow::Load(const FS::FileSystemManager& fileSystem)
    {
        m_Loaded    = true;
        m_Dirty     = false;
        m_Listening = std::nullopt;
        m_FileError.clear();

        const std::optional<std::string> text = fileSystem.ReadTextFile(PATH);
        if (!text)
        {
            m_FileError = "The file cannot be read.";
            m_Actions   = HInput::InputActionSet{};
            return;
        }
        HInput::InputActionParseResult parsed = HInput::ParseInputActions(*text);
        if (!parsed.Actions)
        {
            m_FileError = parsed.Error;
            m_Actions   = HInput::InputActionSet{};
            return;
        }
        m_Actions = std::move(*parsed.Actions);
    }

    void InputActionsWindow::Save(const FS::FileSystemManager& fileSystem)
    {
        if (fileSystem.WriteTextFile(PATH, HInput::WriteInputActions(m_Actions)))
        {
            m_Dirty = false;
            m_FileError.clear();
        }
    }

    std::optional<std::string> InputActionsWindow::FindProblem() const
    {
        for (const auto& [map, mapName] : { std::pair{ &m_Actions.Game, "Game" }, std::pair{ &m_Actions.Editor, "Editor" } })
        {
            std::set<std::string> names;
            for (const HInput::InputAction& action : map->Actions)
            {
                if (action.Name.empty())
                    return std::string(mapName) + ": an action has no name.";
                if (!names.insert(action.Name).second)
                    return std::string(mapName) + ": the action '" + action.Name + "' is defined twice.";
            }
        }
        return std::nullopt;
    }

    void InputActionsWindow::Draw(const FS::FileSystemManager& fileSystem, const HW::RawInput& input)
    {
        if (m_Listening)
            Listen(input);
        m_PreviousInput = input;
        if (!Open)
            return;
        if (!m_Loaded)
            Load(fileSystem);

        ImGui::SetNextWindowSize(ImVec2(620.0f, 520.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Input Actions", &Open))
        {
            ImGui::End();
            return;
        }

        ImGui::TextDisabled("%s%s", PATH, m_Dirty ? " (unsaved)" : "");
        const std::optional<std::string> problem = FindProblem();
        ImGui::BeginDisabled(!m_Dirty || problem.has_value());
        if (ImGui::Button("Save"))
            Save(fileSystem);
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Revert"))
            Load(fileSystem);
        if (problem)
            ImGui::TextColored(ERROR_COLOR, "%s", problem->c_str());

        if (!m_FileError.empty())
        {
            ImGui::TextColored(ERROR_COLOR, "The file does not load: %s", m_FileError.c_str());
            if (ImGui::Button("Reset to defaults"))
            {
                m_Actions = HInput::MakeDefaultInputActions();
                m_Dirty   = true;
                m_FileError.clear();
            }
        }
        if (m_Listening)
            ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.0f, 1.0f), "Press a key or button for the binding (Escape cancels).");

        ImGui::Separator();
        if (ImGui::BeginTabBar("##InputActionMaps"))
        {
            if (ImGui::BeginTabItem("Game"))
            {
                DrawMap(m_Actions.Game, false);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Editor"))
            {
                DrawMap(m_Actions.Editor, true);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();
    }

    void InputActionsWindow::DrawMap(HInput::InputActionMap& map, bool editor)
    {
        std::optional<size_t> remove;
        std::optional<std::pair<size_t, size_t>> swap;
        for (size_t i = 0; i < map.Actions.size(); ++i)
        {
            HInput::InputAction& action = map.Actions[i];
            ImGui::PushID(static_cast<int>(i));

            std::array<char, 128> name{};
            action.Name.copy(name.data(), name.size() - 1);
            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::InputText("##Name", name.data(), name.size()))
            {
                action.Name = name.data();
                m_Dirty     = true;
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(i == 0);
            if (ImGui::ArrowButton("##Up", ImGuiDir_Up))
                swap = std::pair{ i - 1, i };
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(i + 1 == map.Actions.size());
            if (ImGui::ArrowButton("##Down", ImGuiDir_Down))
                swap = std::pair{ i, i + 1 };
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Delete"))
                remove = i;

            ImGui::Indent();
            std::optional<size_t> removeBinding;
            for (size_t b = 0; b < action.Bindings.size(); ++b)
            {
                ImGui::PushID(static_cast<int>(b));
                if (DrawBinding(action.Bindings[b], ListenTarget{ editor, i, b, false }))
                    removeBinding = b;
                ImGui::PopID();
            }
            if (removeBinding)
            {
                action.Bindings.erase(action.Bindings.begin() + static_cast<std::ptrdiff_t>(*removeBinding));
                m_Listening = std::nullopt;
                m_Dirty     = true;
            }
            if (ImGui::SmallButton("Add binding"))
            {
                action.Bindings.push_back(HInput::InputBinding{});
                m_Dirty = true;
            }
            ImGui::Unindent();
            ImGui::Separator();
            ImGui::PopID();
        }

        if (remove)
            map.Actions.erase(map.Actions.begin() + static_cast<std::ptrdiff_t>(*remove));
        if (swap)
            std::swap(map.Actions[swap->first], map.Actions[swap->second]);
        if (remove || swap)
        {
            m_Listening = std::nullopt;
            m_Dirty     = true;
        }

        if (ImGui::Button("Add action"))
        {
            map.Actions.push_back(HInput::InputAction{ "NewAction" + std::to_string(map.Actions.size()), {} });
            m_Dirty = true;
        }
    }

    bool InputActionsWindow::DrawBinding(HInput::InputBinding& binding, const ListenTarget& target)
    {
        ImGui::SetNextItemWidth(130.0f);
        if (ImGui::BeginCombo("##Source", SourceLabel(binding.Source)))
        {
            for (const SourceName& entry : SOURCES)
            {
                if (ImGui::Selectable(entry.Name, entry.Source == binding.Source) && entry.Source != binding.Source)
                {
                    binding.Source       = entry.Source;
                    binding.Code         = 0;
                    binding.NegativeCode = 0;
                    m_Dirty              = true;
                }
            }
            ImGui::EndCombo();
        }

        if (binding.Source == HInput::BindingSource::KeyAxis)
        {
            ImGui::SameLine();
            ListenTarget negative = target;
            negative.Negative     = true;
            m_Dirty |= DrawCodeField("-##Negative", binding.Source, binding.NegativeCode, negative);
            ImGui::SameLine();
            m_Dirty |= DrawCodeField("+##Positive", binding.Source, binding.Code, target);
        }
        else if (binding.Source == HInput::BindingSource::Key || binding.Source == HInput::BindingSource::MouseButton ||
                 binding.Source == HInput::BindingSource::GamepadButton || binding.Source == HInput::BindingSource::GamepadAxis)
        {
            ImGui::SameLine();
            m_Dirty |= DrawCodeField("##Code", binding.Source, binding.Code, target);
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f);
        m_Dirty |= ImGui::DragFloat("Scale", &binding.Scale, 0.05f);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f);
        m_Dirty |= ImGui::DragFloat("Deadzone", &binding.Deadzone, 0.01f, 0.0f, 1000.0f);
        ImGui::SameLine();
        return ImGui::SmallButton("Remove");
    }

    bool InputActionsWindow::DrawCodeField(const char* label, HInput::BindingSource source, uint16_t& code,
                                           const ListenTarget& target)
    {
        const uint16_t before  = code;
        const std::string name = CodeName(source, code);
        ImGui::SetNextItemWidth(120.0f);
        switch (source)
        {
        case HInput::BindingSource::Key:
        case HInput::BindingSource::KeyAxis:
            NameCombo(label, code, name, HInput::GetKeyNames(), [](const HInput::NamedKey& e) { return static_cast<uint16_t>(e.Key); });
            break;
        case HInput::BindingSource::MouseButton:
            NameCombo(label, code, name, HInput::GetMouseButtonNames(),
                      [](const HInput::NamedMouseButton& e) { return static_cast<uint16_t>(e.Button); });
            break;
        case HInput::BindingSource::GamepadButton:
            NameCombo(label, code, name, HInput::GetGamepadButtonNames(),
                      [](const HInput::NamedGamepadButton& e) { return static_cast<uint16_t>(e.Button); });
            break;
        case HInput::BindingSource::GamepadAxis:
            NameCombo(label, code, name, HInput::GetGamepadAxisNames(),
                      [](const HInput::NamedGamepadAxis& e) { return static_cast<uint16_t>(e.Axis); });
            break;
        default:
            break;
        }

        if (source != HInput::BindingSource::GamepadAxis)
        {
            ImGui::SameLine();
            ImGui::PushID(label);
            const bool listening = m_Listening && m_Listening->Editor == target.Editor && m_Listening->Action == target.Action &&
                                   m_Listening->Binding == target.Binding && m_Listening->Negative == target.Negative;
            if (ImGui::SmallButton(listening ? "..." : "Listen"))
                m_Listening = target;
            ImGui::PopID();
        }
        return code != before;
    }

    void InputActionsWindow::Listen(const HW::RawInput& input)
    {
        if (HW::IsKeyDown(input, HW::Key::Escape))
        {
            m_Listening = std::nullopt;
            return;
        }

        HInput::InputActionMap& map = m_Listening->Editor ? m_Actions.Editor : m_Actions.Game;
        if (m_Listening->Action >= map.Actions.size() || m_Listening->Binding >= map.Actions[m_Listening->Action].Bindings.size())
        {
            m_Listening = std::nullopt;
            return;
        }
        HInput::InputBinding& binding = map.Actions[m_Listening->Action].Bindings[m_Listening->Binding];
        uint16_t&             code    = m_Listening->Negative ? binding.NegativeCode : binding.Code;

        // The first key or button that went down since the last frame, of the binding's kind.
        std::optional<uint16_t> pressed;
        switch (binding.Source)
        {
        case HInput::BindingSource::Key:
        case HInput::BindingSource::KeyAxis:
            for (size_t key = 0; key < HW::KEY_COUNT && !pressed; ++key)
            {
                if (input.Keys[key] && !m_PreviousInput.Keys[key] && !HInput::GetKeyName(static_cast<uint16_t>(key)).empty())
                    pressed = static_cast<uint16_t>(key);
            }
            break;
        case HInput::BindingSource::MouseButton:
            for (size_t button = 0; button < HW::MOUSE_BUTTON_COUNT && !pressed; ++button)
            {
                if (input.MouseButtons[button] && !m_PreviousInput.MouseButtons[button])
                    pressed = static_cast<uint16_t>(button);
            }
            break;
        case HInput::BindingSource::GamepadButton:
            for (size_t button = 0; button < HW::GAMEPAD_BUTTON_COUNT && !pressed; ++button)
            {
                if (input.Gamepad.Buttons[button] && !m_PreviousInput.Gamepad.Buttons[button])
                    pressed = static_cast<uint16_t>(button);
            }
            break;
        default:
            m_Listening = std::nullopt;
            return;
        }
        if (pressed)
        {
            code        = *pressed;
            m_Dirty     = true;
            m_Listening = std::nullopt;
        }
    }
}

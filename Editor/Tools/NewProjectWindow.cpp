#include "Tools/NewProjectWindow.hpp"
#include "Tools/NewProjectCheck.hpp"

#include "HedgehogEngine/HedgehogSettings/api/ProjectTemplate.hpp"

#include "DialogueWindows/api/ProjectDialogue.hpp"

#include "FileSystem/api/PathUtils.hpp"
#include "Logger/api/Logger.hpp"

#include "imgui.h"

#include <array>

namespace Editor
{
    namespace
    {
        constexpr const char* DEFAULT_NAME = "MyGame";
        const ImVec4          ERROR_COLOR(0.95f, 0.35f, 0.35f, 1.0f);

        // A text field over a std::string, up to 255 bytes.
        bool InputString(const char* label, std::string& value)
        {
            std::array<char, 256> buffer{};
            value.copy(buffer.data(), buffer.size() - 1);
            if (!ImGui::InputText(label, buffer.data(), buffer.size()))
                return false;
            value = buffer.data();
            return true;
        }
    }

    void NewProjectWindow::Show(const std::filesystem::path& openProject)
    {
        Open     = true;
        m_Name   = DEFAULT_NAME;
        m_Parent = openProject.parent_path().string();
        m_Status.clear();
    }

    std::optional<std::filesystem::path> NewProjectWindow::Draw(bool editing)
    {
        if (!Open)
            return std::nullopt;

        ImGui::SetNextWindowSize(ImVec2(560.0f, 0.0f), ImGuiCond_Appearing);
        if (!ImGui::Begin("New Project", &Open, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::End();
            return std::nullopt;
        }

        if (InputString("Name", m_Name))
            m_Status.clear();
        if (InputString("Parent folder", m_Parent))
            m_Status.clear();
        ImGui::SameLine();
        if (ImGui::Button("Browse..."))
        {
            if (const char* folder = DialogueWindows::ProjectFolderDialogue("Choose the folder to create the project in",
                                                                            m_Parent.c_str()))
                m_Parent = folder;
        }

        const std::string            problem = CheckNewProject(m_Name, m_Parent);
        const std::filesystem::path  folder  = MakeNewProjectFolder(m_Name, m_Parent);
        ImGui::TextDisabled("Creates %s", folder.string().c_str());
        if (!problem.empty())
            ImGui::TextColored(ERROR_COLOR, "%s", problem.c_str());
        if (!m_Status.empty())
            ImGui::TextColored(ERROR_COLOR, "%s", m_Status.c_str());
        if (!editing)
            ImGui::TextDisabled("Stop Play to create a project.");
        ImGui::TextDisabled("The editor then reopens on it; unsaved changes to the scene are lost.");

        std::optional<std::filesystem::path> created;
        ImGui::BeginDisabled(!problem.empty() || !editing);
        if (ImGui::Button("Create"))
        {
            const std::filesystem::path templateDir =
                FS::GetEngineRootDirectory() / HedgehogSettings::EMPTY_TEMPLATE_DIRECTORY;
            m_Status = HedgehogSettings::CreateProject(templateDir, folder, m_Name);
            if (m_Status.empty())
            {
                LOGINFO("[Editor] Created the project '", m_Name, "' at ", folder.string(), ".");
                created = folder;
                Open    = false;
            }
            else
                LOGERROR("[Editor] The project could not be created: ", m_Status);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            Open = false;

        ImGui::End();
        return created;
    }
}

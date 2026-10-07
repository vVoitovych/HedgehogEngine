#include "Tools/BuildGameWindow.hpp"
#include "Tools/GameBinaries.hpp"
#include "Platform/ShellActions.hpp"

#include "DialogueWindows/api/ProjectDialogue.hpp"

#include "FileSystem/api/PathUtils.hpp"
#include "Logger/api/Logger.hpp"

#include "imgui.h"

#include <array>

namespace Editor
{
    namespace
    {
        constexpr const char* BUILD_FOLDER = "Build";
        constexpr const char* GAME_EXECUTABLE = "Game.exe";
        const ImVec4          ERROR_COLOR(0.95f, 0.35f, 0.35f, 1.0f);
        const ImVec4          NOTE_COLOR(0.95f, 0.75f, 0.35f, 1.0f);

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

    void BuildGameWindow::Show(const std::filesystem::path& projectRoot, const std::string& projectName)
    {
        Open          = true;
        m_ProjectRoot = projectRoot;
        m_Output      = (FS::GetEngineRootDirectory() / BUILD_FOLDER / projectName).lexically_normal().string();

        const GameBinariesChoice binaries = ChooseGameBinaries(FS::GetEngineRootDirectory(), FS::GetExecutableDirectory());
        m_Binaries                        = binaries.Path;
        m_BinariesWarning                 = binaries.Warning;
    }

    void BuildGameWindow::Draw()
    {
        if (!Open)
            return;

        ImGui::SetNextWindowSize(ImVec2(620.0f, 0.0f), ImGuiCond_Appearing);
        if (!ImGui::Begin("Build Game", &Open))
        {
            ImGui::End();
            return;
        }

        ImGui::TextDisabled("Project: %s", m_ProjectRoot.string().c_str());
        InputString("Output folder", m_Output);
        ImGui::SameLine();
        if (ImGui::Button("Browse..."))
        {
            if (const char* folder = DialogueWindows::ProjectFolderDialogue("Choose the folder to build the game into",
                                                                            m_Output.c_str()))
                m_Output = folder;
        }
        ImGui::TextDisabled("Binaries: %s", m_Binaries.string().c_str());
        if (!m_BinariesWarning.empty())
            ImGui::TextColored(NOTE_COLOR, "%s", m_BinariesWarning.c_str());

        ImGui::BeginDisabled(m_Output.empty());
        if (ImGui::Button("Build"))
            Build();
        ImGui::EndDisabled();

        if (m_Result)
        {
            ImGui::Separator();
            if (m_Result->Errors.empty())
                ImGui::Text("Built %zu file(s) into %s: %zu copied, %zu unchanged, %zu removed.", m_Result->Files,
                            m_BuiltFolder.string().c_str(), m_Result->Copied, m_Result->Unchanged, m_Result->Removed);
            else
                ImGui::TextColored(ERROR_COLOR, "The build failed; nothing was packaged:");
            for (const std::string& error : m_Result->Errors)
                ImGui::TextColored(ERROR_COLOR, "%s", error.c_str());
            for (const std::string& warning : m_Result->Warnings)
                ImGui::TextColored(NOTE_COLOR, "%s", warning.c_str());

            if (m_Result->Errors.empty())
            {
                if (ImGui::Button("Show in Explorer"))
                    (void)ShowInExplorer(m_BuiltFolder / GAME_EXECUTABLE);
                ImGui::SameLine();
                if (ImGui::Button("Run") && !LaunchDetached(m_BuiltFolder / GAME_EXECUTABLE, m_BuiltFolder))
                    LOGERROR("[Build] ", (m_BuiltFolder / GAME_EXECUTABLE).string(), " could not be started.");
            }
        }
        ImGui::End();
    }

    void BuildGameWindow::Build()
    {
        m_BuiltFolder = std::filesystem::absolute(std::filesystem::path(m_Output)).lexically_normal();
        LOGINFO("[Build] Packaging ", m_ProjectRoot.string(), " into ", m_BuiltFolder.string(), "...");

        Cooker::PackageDesc desc;
        desc.ProjectRoot = m_ProjectRoot;
        desc.EngineRoot  = FS::GetEngineRootDirectory();
        desc.OutDir      = m_BuiltFolder;
        desc.BinariesDir = m_Binaries;
        desc.AllScenes   = true; // every scene ships, as PackageGame.bat packages it
        m_Result         = Cooker::PackageGame(desc);

        for (const std::string& warning : m_Result->Warnings)
            LOGWARNING("[Build] ", warning);
        for (const std::string& error : m_Result->Errors)
            LOGERROR("[Build] ", error);
        if (m_Result->Errors.empty())
            LOGINFO("[Build] Built ", m_Result->Files, " file(s): ", m_Result->Copied, " copied, ", m_Result->Unchanged,
                    " unchanged, ", m_Result->Removed, " removed.");
        else
            LOGERROR("[Build] The build failed; ", m_Result->Errors.size(), " error(s).");
    }
}

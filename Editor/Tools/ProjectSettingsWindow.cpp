#include "ProjectSettingsWindow.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Save/SaveGameManager.hpp"
#include "HedgehogEngine/HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/ProjectSettings.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"

#include "imgui.h"

#include <algorithm>
#include <array>

namespace Editor
{
    namespace
    {
        constexpr const char* SCENE_FOLDER = "assets://Scenes";
        constexpr const char* NO_SCENE     = "(none: Default.yaml)";
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

    void ProjectSettingsWindow::Show(const HedgehogEngine::EngineContext& context)
    {
        Open = true;
        Refresh(context);
    }

    void ProjectSettingsWindow::Refresh(const HedgehogEngine::EngineContext& context)
    {
        m_Scenes.clear();
        if (const auto entries = context.GetFileSystem().ListDirectory(SCENE_FOLDER))
        {
            for (const FS::DirectoryEntry& entry : *entries)
            {
                if (!entry.IsDirectory && entry.Name.ends_with(".yaml"))
                    m_Scenes.push_back(std::string(SCENE_FOLDER) + "/" + entry.Name);
            }
        }
        m_NameBuffer = context.GetSettings().GetProjectSettings().GetName();
    }

    void ProjectSettingsWindow::Draw(HedgehogEngine::EngineContext& context)
    {
        if (!Open)
            return;

        ImGui::SetNextWindowSize(ImVec2(520.0f, 360.0f), ImGuiCond_Appearing);
        if (!ImGui::Begin("Project Settings", &Open))
        {
            ImGui::End();
            return;
        }

        HedgehogSettings::ProjectSettings& project = context.GetSettings().GetProjectSettings();
        ImGui::TextDisabled("%s", HedgehogSettings::ProjectSettings::PATH);

        ImGui::SeparatorText("Project");
        // An invalid name stays in the field, shown in red, and is not applied.
        const bool validName = HedgehogSettings::ProjectSettings::IsValidName(m_NameBuffer);
        if (!validName)
            ImGui::PushStyleColor(ImGuiCol_Text, ERROR_COLOR);
        if (InputString("Name", m_NameBuffer) && HedgehogSettings::ProjectSettings::IsValidName(m_NameBuffer))
            (void)project.SetName(m_NameBuffer);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Saves go in %%LOCALAPPDATA%%/HedgehogEngine/<name>/Saves.");
        if (!validName)
        {
            ImGui::PopStyleColor();
            ImGui::TextColored(ERROR_COLOR, "1 to 64 of A-Z, a-z, 0-9, '_', '-' and inner spaces.");
        }

        const std::string& startup = project.GetStartupScene();
        if (ImGui::BeginCombo("Startup scene", startup.empty() ? NO_SCENE : startup.c_str()))
        {
            if (ImGui::Selectable(NO_SCENE, startup.empty()))
                (void)project.SetStartupScene("");
            for (const std::string& scene : m_Scenes)
            {
                if (ImGui::Selectable(scene.c_str(), scene == startup))
                    (void)project.SetStartupScene(scene);
            }
            ImGui::EndCombo();
        }
        if (!startup.empty() && std::find(m_Scenes.begin(), m_Scenes.end(), startup) == m_Scenes.end())
            ImGui::TextColored(ERROR_COLOR, "%s is not in %s.", startup.c_str(), SCENE_FOLDER);

        int dataVersion = project.GetGameDataVersion();
        if (ImGui::InputInt("Game data version", &dataVersion))
            project.SetGameDataVersion(dataVersion);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Bump it when what the game saves changes shape: older saves are migrated, newer ones refused.");

        ImGui::SeparatorText("Game window");
        std::string title = project.GetWindowTitle();
        if (InputString("Title", title))
            project.SetWindowTitle(title);
        int size[2] = { static_cast<int>(project.GetWindowWidth()), static_cast<int>(project.GetWindowHeight()) };
        if (ImGui::InputInt2("Size", size))
            project.SetWindowSize(static_cast<uint32_t>(std::max(size[0], 0)), static_cast<uint32_t>(std::max(size[1], 0)));
        bool fullscreen = project.IsFullscreen();
        if (ImGui::Checkbox("Fullscreen", &fullscreen))
            project.SetFullscreen(fullscreen);
        ImGui::SameLine();
        bool vsync = project.IsVSync();
        if (ImGui::Checkbox("VSync", &vsync))
            project.SetVSync(vsync);

        ImGui::Separator();
        if (ImGui::Button("Save"))
            Save(context);
        ImGui::SameLine();
        if (ImGui::Button("Revert"))
            Revert(context);
        ImGui::SameLine();
        if (project.IsDirty())
            ImGui::TextUnformatted("Unsaved changes");
        else if (!m_Status.empty())
            ImGui::TextDisabled("%s", m_Status.c_str());

        ImGui::End();
    }

    void ProjectSettingsWindow::Save(HedgehogEngine::EngineContext& context)
    {
        HedgehogSettings::ProjectSettings& project = context.GetSettings().GetProjectSettings();
        if (!project.Save(HedgehogSettings::ProjectSettings::PATH, context.GetFileSystem()))
        {
            m_Status = "Could not save; see the Console.";
            return;
        }
        // A renamed project saves its play sessions in its own folder from now on.
        if (const auto saves = FS::GetSavesDirectory(project.GetName(), true))
            context.GetSaveGames().SetSaveDirectory(*saves);
        m_Status = "Saved.";
    }

    void ProjectSettingsWindow::Revert(HedgehogEngine::EngineContext& context)
    {
        HedgehogSettings::ProjectSettings& project = context.GetSettings().GetProjectSettings();
        if (project.Load(HedgehogSettings::ProjectSettings::PATH, context.GetFileSystem()))
            m_Status = "Reverted to the saved file.";
        else
            m_Status = "No saved file to revert to.";
        Refresh(context);
    }
}

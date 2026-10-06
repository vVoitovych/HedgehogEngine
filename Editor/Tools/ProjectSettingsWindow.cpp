#include "ProjectSettingsWindow.hpp"
#include "PluginNameCheck.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Plugins/PluginManager.hpp"
#include "HedgehogEngine/api/Save/SaveGameManager.hpp"
#include "HedgehogEngine/HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/ProjectSettings.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>

namespace Editor
{
    namespace
    {
        constexpr const char* SCENE_FOLDER = "assets://Scenes";
        constexpr const char* NO_SCENE     = "(none: Default.yaml)";
        const ImVec4          ERROR_COLOR(0.95f, 0.35f, 0.35f, 1.0f);
        const ImVec4          NOTE_COLOR(0.95f, 0.75f, 0.35f, 1.0f);

        bool SameNameIgnoringCase(const std::string& a, const std::string& b)
        {
            return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                              [](unsigned char x, unsigned char y) { return std::tolower(x) == std::tolower(y); });
        }

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
        // A list saved during Play applies once Play has stopped, whether the window is open or not.
        if (m_PendingPlugins && context.GetPlayState() == HedgehogEngine::PlayState::Edit)
        {
            const HedgehogSettings::ProjectSettings saved = std::move(*m_PendingPlugins);
            m_PendingPlugins.reset();
            ApplyPlugins(context, saved);
        }

        if (!Open)
            return;

        ImGui::SetNextWindowSize(ImVec2(560.0f, 520.0f), ImGuiCond_Appearing);
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

        DrawPlugins(context);

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

        // Plugins load and unload in Edit mode only: in Play the saved list waits for Stop.
        if (context.GetPlayState() == HedgehogEngine::PlayState::Edit)
        {
            m_PendingPlugins.reset();
            ApplyPlugins(context, project);
        }
        else
        {
            m_PendingPlugins = project;
            m_Status         = "Saved; the plugins change when Play stops.";
        }
    }

    void ProjectSettingsWindow::ApplyPlugins(HedgehogEngine::EngineContext& context, const HedgehogSettings::ProjectSettings& saved)
    {
        const size_t failed = context.GetPlugins().ApplyProjectPlugins(saved);
        if (failed > 0)
            m_Status += " " + std::to_string(failed) + " plugin(s) did not load; see Plugins.";
    }

    void ProjectSettingsWindow::DrawPlugins(HedgehogEngine::EngineContext& context)
    {
        HedgehogSettings::ProjectSettings& project = context.GetSettings().GetProjectSettings();
        HedgehogEngine::PluginManager&     manager = context.GetPlugins();
        const std::vector<HedgehogEngine::LoadedPlugin> loaded = manager.GetLoaded();

        ImGui::SeparatorText("Plugins");
        if (project.GetPlugins().empty())
            ImGui::TextDisabled("None. A plugin is a DLL beside HedgehogEngine.dll; add its name below.");

        // Changes go to the project; Save loads and unloads to match. No plugin code runs here.
        std::optional<std::string> removed;
        if (!project.GetPlugins().empty() &&
            ImGui::BeginTable("##Plugins", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Plugin", ImGuiTableColumnFlags_WidthStretch, 0.35f);
            ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthStretch, 0.65f);
            ImGui::TableSetupColumn("##Remove", ImGuiTableColumnFlags_WidthFixed);
            const std::vector<HedgehogSettings::PluginEntry> plugins = project.GetPlugins();
            for (const HedgehogSettings::PluginEntry& plugin : plugins)
            {
                ImGui::PushID(plugin.Name.c_str());
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                bool enabled = plugin.Enabled;
                if (ImGui::Checkbox(plugin.Name.c_str(), &enabled))
                    (void)project.SetPluginEnabled(plugin.Name, enabled);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Loaded by the Editor, --game-mode and the game when enabled.");

                ImGui::TableNextColumn();
                const auto running = std::find_if(loaded.begin(), loaded.end(), [&plugin](const HedgehogEngine::LoadedPlugin& entry)
                                                  { return SameNameIgnoringCase(entry.Name, plugin.Name); });
                const std::string error = manager.GetLastError(plugin.Name);
                if (running != loaded.end())
                    ImGui::Text("Loaded %s", running->Version.c_str());
                else if (!error.empty())
                    ImGui::TextColored(ERROR_COLOR, "%s", error.c_str());
                else
                    ImGui::TextDisabled("Not loaded");
                if (!error.empty() && ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", error.c_str());

                ImGui::TableNextColumn();
                if (ImGui::SmallButton("Remove"))
                    removed = plugin.Name;
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (removed)
            (void)project.RemovePlugin(*removed);

        // Add: refused while the name is not one or is listed already.
        const std::string problem = CheckNewPluginName(m_PluginNameBuffer, project.GetPlugins());
        const bool        typed   = !m_PluginNameBuffer.empty();
        if (typed && !problem.empty())
            ImGui::PushStyleColor(ImGuiCol_Text, ERROR_COLOR);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
        (void)InputString("##NewPlugin", m_PluginNameBuffer);
        if (typed && !problem.empty())
            ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::BeginDisabled(!problem.empty());
        if (ImGui::Button("Add plugin") && project.AddPlugin(m_PluginNameBuffer, true))
            m_PluginNameBuffer.clear();
        ImGui::EndDisabled();
        if (typed && !problem.empty())
            ImGui::TextColored(ERROR_COLOR, "%s", problem.c_str());
        else if (typed)
        {
            const std::filesystem::path dll = manager.GetDirectory() / (m_PluginNameBuffer + ".dll");
            std::error_code             error;
            if (!std::filesystem::is_regular_file(dll, error))
                ImGui::TextColored(NOTE_COLOR, "There is no %s yet; it will not load until it is built.", dll.filename().string().c_str());
        }

        if (m_PendingPlugins)
            ImGui::TextColored(NOTE_COLOR, "Saved during Play: the plugins change when Play stops.");
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

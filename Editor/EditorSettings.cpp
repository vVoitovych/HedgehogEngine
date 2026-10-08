#include "EditorSettings.hpp"

#include "yaml-cpp/yaml.h"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <fstream>
#include <string>
#include <system_error>

namespace Editor
{
    namespace
    {
        // Must match DockArea enum order; index 5 = Floating
        constexpr const char* k_DockAreaKeys[] =
            { "left", "top_center", "center", "bottom", "right", "floating" };

        // Areas that are fully serialised (Left/Right/Bottom/Floating)
        constexpr DockArea k_SavedAreas[] =
            { DockArea::Left, DockArea::Right, DockArea::Bottom, DockArea::Floating };
    }

    void EditorSettings::Save(const std::string& virtualPath,
                               const FS::FileSystemManager& fileSystem) const
    {
        YAML::Emitter out;
        out << YAML::BeginMap;

        out << YAML::Key << "dock_layout" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "left_width"    << YAML::Value << dockLayout.LeftWidth;
        out << YAML::Key << "right_width"   << YAML::Value << dockLayout.RightWidth;
        out << YAML::Key << "bottom_height" << YAML::Value << dockLayout.BottomHeight;

        // Area → panel lists and active tab indices
        out << YAML::Key << "areas" << YAML::Value << YAML::BeginMap;
        for (const DockArea area : k_SavedAreas)
        {
            const int areaIdx = static_cast<int>(area);
            out << YAML::Key << k_DockAreaKeys[areaIdx]
                << YAML::Value << YAML::Flow << YAML::BeginSeq;
            for (const PanelId pid : dockLayout.AreaPanels[areaIdx])
                out << PanelIdToString(pid);
            out << YAML::EndSeq;

            out << YAML::Key << (std::string(k_DockAreaKeys[areaIdx]) + "_active")
                << YAML::Value << dockLayout.ActiveTab[areaIdx];
        }
        out << YAML::EndMap; // areas

        // Per-panel visibility
        out << YAML::Key << "panel_visible" << YAML::Value << YAML::BeginMap;
        for (int i = 0; i < PANEL_ID_COUNT; ++i)
            out << YAML::Key << PanelIdToString(static_cast<PanelId>(i))
                << YAML::Value << dockLayout.PanelVisible[i];
        out << YAML::EndMap; // panel_visible

        // Floating panel last-known positions
        out << YAML::Key << "floating_positions" << YAML::Value << YAML::BeginMap;
        for (int i = 0; i < PANEL_ID_COUNT; ++i)
        {
            const auto& p = dockLayout.FloatingPositions[i];
            out << YAML::Key << PanelIdToString(static_cast<PanelId>(i))
                << YAML::Value << YAML::Flow << YAML::BeginSeq
                << p.x << p.y
                << YAML::EndSeq;
        }
        out << YAML::EndMap; // floating_positions

        out << YAML::EndMap; // dock_layout

        out << YAML::Key << "content_icon_size" << YAML::Value << ContentIconSize;
        out << YAML::Key << "physics_debug" << YAML::Value << PhysicsDebug;

        out << YAML::Key << "recent_projects" << YAML::Value << YAML::BeginSeq;
        for (const RecentProject& project : RecentProjects)
            out << YAML::BeginMap << YAML::Key << "path" << YAML::Value << project.Path.generic_string()
                << YAML::Key << "last_scene" << YAML::Value << project.LastScene << YAML::EndMap;
        out << YAML::EndSeq;

        out << YAML::EndMap; // root

        const std::optional<std::filesystem::path> path = fileSystem.ResolvePhysical(virtualPath);
        if (!path)
        {
            LOGERROR("EditorSettings::Save: '", virtualPath, "' is not under a mount.");
            return;
        }
        std::filesystem::path temporary = *path;
        temporary += ".tmp";
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            file << out.c_str() << '\n';
            if (!file)
            {
                LOGERROR("EditorSettings::Save: failed to write '", temporary.string(), "'.");
                return;
            }
        }
        std::error_code error;
        std::filesystem::rename(temporary, *path, error);
        if (error)
            LOGERROR("EditorSettings::Save: failed to replace '", path->string(), "': ", error.message());
    }

    bool EditorSettings::Load(const std::string& virtualPath, const FS::FileSystemManager& fileSystem)
    {
        const auto text = fileSystem.ReadTextFile(virtualPath);
        if (!text)
            return false;

        try
        {
            YAML::Node root = YAML::Load(*text);

            // Missing: the default; out of range: clamped.
            ContentIconSize = CONTENT_ICON_SIZE_DEFAULT;
            if (const YAML::Node size = root["content_icon_size"])
                ContentIconSize = std::clamp(size.as<float>(CONTENT_ICON_SIZE_DEFAULT), CONTENT_ICON_SIZE_MIN, CONTENT_ICON_SIZE_MAX);
            PhysicsDebug = false;
            if (const YAML::Node debug = root["physics_debug"])
                PhysicsDebug = debug.as<bool>(false);

            if (auto dock = root["dock_layout"])
            {
                // The file's layout over the defaults, so a panel it does not know keeps its own.
                dockLayout.InitDefaults();
                if (auto n = dock["left_width"])    dockLayout.LeftWidth    = n.as<float>();
                if (auto n = dock["right_width"])   dockLayout.RightWidth   = n.as<float>();
                if (auto n = dock["bottom_height"]) dockLayout.BottomHeight = n.as<float>();

                if (auto areas = dock["areas"])
                {
                    for (const DockArea area : k_SavedAreas)
                    {
                        const int areaIdx = static_cast<int>(area);
                        if (auto seq = areas[k_DockAreaKeys[areaIdx]])
                        {
                            dockLayout.AreaPanels[areaIdx].clear();
                            for (const auto& node : seq)
                            {
                                if (auto pid = StringToPanelId(node.as<std::string>()))
                                    dockLayout.AreaPanels[areaIdx].push_back(pid.value());
                            }
                        }
                        const std::string activeKey =
                            std::string(k_DockAreaKeys[areaIdx]) + "_active";
                        if (auto n = areas[activeKey])
                            dockLayout.ActiveTab[areaIdx] = n.as<int>();
                    }
                }

                // A panel newer than the file has no visibility entry: it is shown, and placed in
                // its default area when the file's area lists left it out.
                const YAML::Node vis = dock["panel_visible"];
                for (int i = 0; i < PANEL_ID_COUNT; ++i)
                {
                    const PanelId id = static_cast<PanelId>(i);
                    if (vis && vis[PanelIdToString(id)])
                        dockLayout.PanelVisible[i] = vis[PanelIdToString(id)].as<bool>();
                    else if (!dockLayout.IsPanelInAnyArea(id))
                        dockLayout.AreaPanels[static_cast<int>(DefaultPanelArea(id))].push_back(id);
                }

                if (auto fp = dock["floating_positions"])
                {
                    for (int i = 0; i < PANEL_ID_COUNT; ++i)
                    {
                        const char* key = PanelIdToString(static_cast<PanelId>(i));
                        if (auto seq = fp[key])
                        {
                            if (seq.IsSequence() && seq.size() == 2)
                            {
                                dockLayout.FloatingPositions[i].x = seq[0].as<float>();
                                dockLayout.FloatingPositions[i].y = seq[1].as<float>();
                            }
                        }
                    }
                }
            }
            else
            {
                dockLayout.InitDefaults();
            }

            // An entry without a path is skipped; the list is kept as written, at most the limit.
            RecentProjects.clear();
            if (const YAML::Node recent = root["recent_projects"]; recent && recent.IsSequence())
            {
                for (const YAML::Node& entry : recent)
                {
                    const std::string folder = entry["path"] ? entry["path"].as<std::string>("") : "";
                    if (folder.empty() || FindRecentProject(RecentProjects, folder) ||
                        RecentProjects.size() == MAX_RECENT_PROJECTS)
                        continue;
                    RecentProjects.push_back({ NormalizeProjectPath(folder),
                                               entry["last_scene"] ? entry["last_scene"].as<std::string>("") : "" });
                }
            }

            return true;
        }
        catch (...) { return false; }
    }
}

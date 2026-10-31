#include "ProfilerWindow.hpp"

#include "EditorTheme.hpp"
#include "Panels/TextSearch.hpp"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

// The Profiler's details pane, as Unity's: for CPU Usage a Timeline, Hierarchy or Inverted Hierarchy
// view of the shown frame, a thread selector (the GPU among the threads), a search and the selected
// sample; for GPU Usage the GPU thread's hierarchy, and for Rendering its counters too.
namespace Editor
{
    namespace
    {
        constexpr const char* VIEW_NAMES[] = { "Timeline", "Hierarchy", "Inverted Hierarchy" };

        // The thread the Hierarchy views show: the one named (the GPU's while the frame has one),
        // else the first.
        const Profiling::ThreadRecord* FindThread(const Profiling::FrameRecord& frame, const Profiling::ThreadRecord* gpu,
                                                  const std::string& name)
        {
            if (gpu != nullptr && gpu->ThreadName == name)
                return gpu;
            for (const Profiling::ThreadRecord& thread : frame.Threads)
            {
                if (thread.ThreadName == name)
                    return &thread;
            }
            return frame.Threads.empty() ? nullptr : &frame.Threads.front();
        }

        // The category a zone's time counts towards: its own, else its nearest ancestor's (the zones
        // around it on its thread), else Others, as ComputeCategoryTimes counts it.
        ZoneCategory FindCategory(const Profiling::ThreadRecord& thread, const Profiling::ZoneRecord& zone)
        {
            if (const std::optional<ZoneCategory> own = CategorizeZone(zone.Name ? zone.Name : ""))
                return *own;
            const Profiling::ZoneRecord* nearest = nullptr;
            for (const Profiling::ZoneRecord& other : thread.Zones)
            {
                const bool around = other.Depth < zone.Depth && other.StartNs <= zone.StartNs &&
                                    other.StartNs + other.DurationNs >= zone.StartNs + zone.DurationNs;
                if (around && CategorizeZone(other.Name ? other.Name : "") && (nearest == nullptr || other.Depth > nearest->Depth))
                    nearest = &other;
            }
            return nearest != nullptr ? *CategorizeZone(nearest->Name) : ZoneCategory::Others;
        }
    }

    // Under the module charts: a row of the view, the thread and the search with the frame's CPU and
    // GPU times at its end, then the active module's details.
    void ProfilerWindow::DrawDetails(const Profiling::FrameRecord& frame, const GpuSample* gpu)
    {
        const bool                     cpu       = m_ActiveModule == ProfilerModule::CpuUsage;
        const Profiling::ThreadRecord* gpuThread = gpu != nullptr ? &gpu->Thread : nullptr;
        if (cpu)
        {
            ImGui::SetNextItemWidth(ImGui::CalcTextSize("Inverted Hierarchy").x + ImGui::GetFrameHeight() * 2.0f);
            if (ImGui::BeginCombo("##DetailsView", VIEW_NAMES[static_cast<size_t>(m_DetailsView)]))
            {
                for (size_t i = 0; i < std::size(VIEW_NAMES); ++i)
                {
                    if (ImGui::Selectable(VIEW_NAMES[i], static_cast<size_t>(m_DetailsView) == i))
                        m_DetailsView = static_cast<DetailsView>(i);
                }
                ImGui::EndCombo();
            }
            if (m_DetailsView != DetailsView::Timeline)
            {
                ImGui::SameLine();
                const Profiling::ThreadRecord* thread = FindThread(frame, gpuThread, m_DetailsThread);
                ImGui::SetNextItemWidth(160.0f);
                if (ImGui::BeginCombo("##DetailsThread", thread != nullptr ? thread->ThreadName.c_str() : "No thread"))
                {
                    for (const Profiling::ThreadRecord& option : frame.Threads)
                    {
                        if (ImGui::Selectable(option.ThreadName.c_str(), &option == thread))
                            m_DetailsThread = option.ThreadName;
                    }
                    if (gpuThread != nullptr && ImGui::Selectable(GPU_THREAD_NAME, gpuThread == thread))
                        m_DetailsThread = GPU_THREAD_NAME;
                    ImGui::EndCombo();
                }
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(200.0f);
            ImGui::InputTextWithHint("##DetailsSearch", "Search", m_Search, sizeof(m_Search));
        }
        else
            ImGui::TextUnformatted(m_ActiveModule == ProfilerModule::GpuUsage ? "GPU Usage" : "Rendering");

        char times[64];
        if (gpu != nullptr)
            std::snprintf(times, sizeof(times), "CPU: %.3f ms   GPU: %.3f ms", GetFrameMs(frame), gpu->FrameMs);
        else
            std::snprintf(times, sizeof(times), "CPU: %.3f ms   GPU: -", GetFrameMs(frame));
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(times).x));
        ImGui::TextUnformatted(times);

        if (!cpu)
        {
            // The GPU thread's hierarchy, its times shares of the GPU frame; Rendering's counters first.
            if (gpu == nullptr || !gpu->HasCommands)
                ImGui::TextDisabled("No timed GPU commands for this frame yet: they arrive a few frames after its CPU zones.");
            if (gpu == nullptr)
                return;
            if (m_ActiveModule == ProfilerModule::Rendering)
                DrawRenderingCounters(*gpu);
            DrawHierarchy(gpu->Thread, gpu->FrameMs, false, "");
            return;
        }
        DrawSelectedSample(frame, gpu);
        if (m_DetailsView == DetailsView::Timeline)
        {
            // Ctrl+wheel zooms the timeline, so it must not scroll it too.
            if (ImGui::BeginChild("##ProfilerTimeline", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders,
                                  ImGui::GetIO().KeyCtrl ? ImGuiWindowFlags_NoScrollWithMouse : ImGuiWindowFlags_None))
                DrawTimeline(frame);
            ImGui::EndChild();
        }
        else if (const Profiling::ThreadRecord* thread = FindThread(frame, gpuThread, m_DetailsThread))
            DrawHierarchy(*thread, thread == gpuThread ? gpu->FrameMs : GetFrameMs(frame),
                          m_DetailsView == DetailsView::InvertedHierarchy, m_Search);
        else
            ImGui::TextDisabled("No zones in this frame.");
    }

    // The sample picked on the timeline, while its frame is shown: its name, time, category and how
    // often its name ran in the frame (on the GPU, how often and how long on the GPU).
    void ProfilerWindow::DrawSelectedSample(const Profiling::FrameRecord& frame, const GpuSample* gpu)
    {
        if (m_SelectedSample.FrameIndex != frame.Index || m_DetailsView != DetailsView::Timeline)
            return;
        if (gpu != nullptr && m_SelectedSample.ThreadIndex == GPU_THREAD_INDEX)
        {
            uint32_t calls = 0;
            double   ms    = 0.0;
            for (const Profiling::ZoneRecord& zone : gpu->Thread.Zones)
            {
                if (std::strcmp(zone.Name, m_SelectedSample.Name) == 0)
                {
                    ++calls;
                    ms += static_cast<double>(zone.DurationNs) / 1.0e6;
                }
            }
            for (const Profiling::ZoneRecord& zone : gpu->Thread.Zones)
            {
                if (zone.StartNs == m_SelectedSample.StartNs && std::strcmp(zone.Name, m_SelectedSample.Name) == 0)
                {
                    ImGui::Text("Selected: %s   %.3f ms   GPU: %u call%s, %.3f ms in the frame", zone.Name,
                                static_cast<double>(zone.DurationNs) / 1.0e6, calls, calls == 1 ? "" : "s", ms);
                    return;
                }
            }
            return;
        }
        for (const Profiling::ThreadRecord& thread : frame.Threads)
        {
            if (thread.ThreadIndex != m_SelectedSample.ThreadIndex)
                continue;
            for (const Profiling::ZoneRecord& zone : thread.Zones)
            {
                if (zone.StartNs != m_SelectedSample.StartNs || std::strcmp(zone.Name, m_SelectedSample.Name) != 0)
                    continue;
                ComputeTopZones(frame, m_TopZones);
                const auto stat = std::find_if(m_TopZones.begin(), m_TopZones.end(), [&](const ZoneStat& s) { return s.Name == zone.Name; });
                const uint32_t calls = stat != m_TopZones.end() ? stat->Calls : 1u;
                ImGui::Text("Selected: %s   %.3f ms   %s   %s: %u call%s, %.3f ms in the frame", zone.Name,
                            static_cast<double>(zone.DurationNs) / 1.0e6, GetZoneCategoryName(FindCategory(thread, zone)),
                            thread.ThreadName.c_str(), calls, calls == 1 ? "" : "s",
                            stat != m_TopZones.end() ? stat->TotalMs : 0.0);
                return;
            }
        }
    }

    // Unity's Hierarchy and Inverted Hierarchy: the thread's zones as a tree table, sortable by any
    // column, its percentages of frameMs; a search lists the matching rows flat.
    void ProfilerWindow::DrawHierarchy(const Profiling::ThreadRecord& thread, double frameMs, bool inverted, const char* search)
    {
        if (inverted)
            BuildInvertedHierarchy(thread, frameMs, m_Tree);
        else
            BuildHierarchy(thread, frameMs, m_Tree);

        constexpr ImGuiTableFlags FLAGS = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuter |
                                          ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable |
                                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
        // No saved settings: ImGui reloads a saved sort without its column order, moving that column first.
        if (!ImGui::BeginTable("##Hierarchy", 6, FLAGS))
            return;
        ImGui::TableSetupScrollFreeze(0, 1);
        constexpr ImGuiTableColumnFlags COLUMN = ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoReorder;
        constexpr ImGuiTableColumnFlags NUMBER = COLUMN | ImGuiTableColumnFlags_PreferSortDescending;
        ImGui::TableSetupColumn("Overview", COLUMN | ImGuiTableColumnFlags_NoHide, 4.0f);
        ImGui::TableSetupColumn("Total", NUMBER, 1.0f);
        ImGui::TableSetupColumn("Self", NUMBER, 1.0f);
        ImGui::TableSetupColumn("Calls", NUMBER, 0.8f);
        ImGui::TableSetupColumn("Time ms", NUMBER | ImGuiTableColumnFlags_DefaultSort, 1.0f);
        ImGui::TableSetupColumn("Self ms", NUMBER, 1.0f);
        ImGui::TableHeadersRow();

        // Overview sorts by name; Total and Time ms by total; Self and Self ms by self time.
        if (const ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs != nullptr && specs->SpecsCount > 0)
        {
            static constexpr ProfilerSortKey KEYS[] = { ProfilerSortKey::Name, ProfilerSortKey::Total, ProfilerSortKey::Self,
                                                        ProfilerSortKey::Calls, ProfilerSortKey::Total, ProfilerSortKey::Self };
            const ImGuiTableColumnSortSpecs& spec = specs->Specs[0];
            const ProfilerSortKey            key  = KEYS[std::min<size_t>(spec.ColumnIndex, std::size(KEYS) - 1)];
            // The tree's own order is names A to Z and numbers largest first.
            const bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
            SortProfilerTree(m_Tree, key, key == ProfilerSortKey::Name ? !ascending : ascending);
        }

        if (search[0] != '\0' && inverted)
        {
            // Inverted, each name is one top-level node (those below it are its callers).
            ForEachChild(m_Tree, 0, [&](uint32_t child)
            {
                if (ContainsIgnoringCase(m_Tree.Nodes[child].Name, search))
                    DrawTreeRow(child, false);
            });
        }
        else if (search[0] != '\0')
        {
            // Flat: every node matching, in the tree's order.
            std::vector<uint32_t>& stack = m_TreeStack;
            stack.clear();
            stack.push_back(0);
            while (!stack.empty())
            {
                const uint32_t node = stack.back();
                stack.pop_back();
                if (node != 0 && ContainsIgnoringCase(m_Tree.Nodes[node].Name, search))
                    DrawTreeRow(node, false);
                const size_t first = stack.size();
                ForEachChild(m_Tree, node, [&](uint32_t child) { stack.push_back(child); });
                std::reverse(stack.begin() + static_cast<std::ptrdiff_t>(first), stack.end());
            }
        }
        else
            ForEachChild(m_Tree, 0, [&](uint32_t child) { DrawTreeRow(child, true); });
        ImGui::EndTable();
    }

    // One node's row and, while it is open in a tree, its children's.
    void ProfilerWindow::DrawTreeRow(uint32_t node, bool tree)
    {
        const ProfilerNode& entry = m_Tree.Nodes[node];
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::PushID(static_cast<int>(node));
        const bool leaf = !tree || entry.FirstChild == NO_PROFILER_NODE;
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAllColumns;
        if (leaf)
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        // Each tree opens to its first level, as Unity's does.
        if (entry.Parent == 0)
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        const bool open = ImGui::TreeNodeEx(entry.Name != nullptr ? entry.Name : "", flags);
        const double frameMs = std::max(m_Tree.FrameMs, 1.0e-9);
        ImGui::TableNextColumn();
        ImGui::Text("%.1f%%", entry.TotalMs / frameMs * 100.0);
        ImGui::TableNextColumn();
        ImGui::Text("%.1f%%", entry.SelfMs / frameMs * 100.0);
        ImGui::TableNextColumn();
        ImGui::Text("%u", entry.Calls);
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", entry.TotalMs);
        ImGui::TableNextColumn();
        ImGui::Text("%.3f", entry.SelfMs);
        if (open && !leaf)
        {
            ForEachChild(m_Tree, node, [&](uint32_t child) { DrawTreeRow(child, true); });
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    // Unity's Rendering counters for the frame, over the GPU thread's hierarchy.
    void ProfilerWindow::DrawRenderingCounters(const GpuSample& gpu)
    {
        const RenderingCounters counts = CountRendering(gpu.Events);
        if (ImGui::BeginTable("##RenderingCounters", 3, ImGuiTableFlags_SizingStretchSame))
        {
            for (size_t c = 0; c < RENDERING_COUNTER_COUNT; ++c)
            {
                ImGui::TableNextColumn();
                ImGui::Text("%s: %s", GetRenderingCounterName(static_cast<RenderingCounter>(c)), FormatCount(counts[c]).c_str());
            }
            ImGui::EndTable();
        }
        ImGui::Spacing();
    }
}

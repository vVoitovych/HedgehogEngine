#include "ProfilerWindow.hpp"
#include "ChromeTrace.hpp"

#include "EditorTheme.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogRenderer/Renderer.hpp"

#include "DialogueWindows/api/ProfilerTraceDialogue.hpp"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <span>
#include <string_view>

namespace Editor
{
    namespace
    {
        constexpr float  CHART_HEIGHT    = 90.0f;
        constexpr float  LEGEND_WIDTH    = 230.0f;
        constexpr ImVec4 CHART_BG        = Theme::FromHex(0x1E2124);
        constexpr ImVec4 RECORD_RED      = Theme::FromHex(0xD9534F);
        constexpr float  ROW_HEIGHT      = 18.0f;
        constexpr double FRAME_BUDGET_MS = 1000.0 / 60.0;
        constexpr double MIN_GRAPH_MS    = 2.0 * FRAME_BUDGET_MS;
        constexpr double MIN_GPU_GRAPH_MS = 0.5;
        constexpr size_t TOP_ZONE_ROWS   = 60;
        constexpr float  RULER_HEIGHT    = 20.0f;
        constexpr float  MIN_TICK_PIXELS = 90.0f;
        constexpr double ZOOM_PER_NOTCH  = 1.25;

        // The frame graph's top: the slowest kept frame (at least two budgets), or a fixed time.
        struct GraphScale
        {
            const char* Label;
            double      Ms; // 0 for Auto
        };
        constexpr GraphScale GRAPH_SCALES[] = { { "Auto", 0.0 }, { "16.6 ms", FRAME_BUDGET_MS },
                                                { "33.3 ms", 2.0 * FRAME_BUDGET_MS }, { "66.7 ms", 4.0 * FRAME_BUDGET_MS } };

        constexpr const char* MODULE_NAMES[] = { "CPU Usage", "GPU Usage", "Rendering" };
        static_assert(std::size(MODULE_NAMES) == PROFILER_MODULE_COUNT);

        // The Rendering module's counters, from a frame's timed GPU commands, and their colours.
        constexpr const char* RENDERING_COUNTERS[] = { "Draw calls", "Commands" };
        constexpr uint32_t    RENDERING_COLORS[]   = { 0x6FB34A, 0x4A90D9 };

        double GetRenderingCounter(std::span<const Renderer::GpuCommandTiming> commands, size_t counter)
        {
            if (counter == 1)
                return static_cast<double>(commands.size());
            return static_cast<double>(std::count_if(commands.begin(), commands.end(), [](const Renderer::GpuCommandTiming& command)
            {
                return command.Kind == Renderer::GpuCommandKind::Draw || command.Kind == Renderer::GpuCommandKind::DrawIndexed;
            }));
        }

        std::string FormatValue(double value, bool count)
        {
            char text[32];
            std::snprintf(text, sizeof(text), count ? "%.0f" : "%.2f ms", value);
            return text;
        }

        ImU32 CategoryColor(ZoneCategory category)
        {
            return ImGui::GetColorU32(Theme::Resolve(Theme::FromHex(GetZoneCategoryColor(category))));
        }

        // The thread the CPU Usage chart shows: the main one, as Unity's does.
        const Profiling::ThreadRecord* FindMainThread(const Profiling::FrameRecord& frame)
        {
            for (const Profiling::ThreadRecord& thread : frame.Threads)
            {
                if (thread.ThreadName == "Main")
                    return &thread;
            }
            return frame.Threads.empty() ? nullptr : &frame.Threads.front();
        }

        // Unity's Record button: a red dot while recording.
        bool RecordButton(bool recording)
        {
            const char*  label   = "Record";
            const float  height  = ImGui::GetFrameHeight();
            const ImVec2 padding = ImGui::GetStyle().FramePadding;
            if (recording)
                ImGui::PushStyleColor(ImGuiCol_Button, Theme::Resolve(Theme::SELECTION_STRONG));
            const bool clicked = ImGui::Button("##Record", ImVec2(height + ImGui::CalcTextSize(label).x + padding.x, height));
            if (recording)
                ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(recording ? "Recording: click to stop and keep these frames." : "Stopped: click to record.");
            const ImVec2 min      = ImGui::GetItemRectMin();
            ImDrawList&  drawList = *ImGui::GetWindowDrawList();
            drawList.AddCircleFilled(ImVec2(min.x + height * 0.5f, min.y + height * 0.5f), height * 0.22f,
                                     ImGui::GetColorU32(Theme::Resolve(recording ? RECORD_RED : Theme::TEXT_MUTED)));
            drawList.AddText(ImVec2(min.x + height, min.y + padding.y), ImGui::GetColorU32(Theme::Resolve(Theme::TEXT)), label);
            return clicked;
        }

        // One legend line: the series' colour (filled while shown), its name and its value; a click
        // on it says to show or hide the series.
        bool LegendEntry(const char* name, ImU32 color, bool visible, double value, bool count)
        {
            ImGui::PushID(name);
            const ImVec2 pos  = ImGui::GetCursorScreenPos();
            const float  line = ImGui::GetTextLineHeight();
            const float  width = std::max(ImGui::GetContentRegionAvail().x, 1.0f);
            const bool   clicked = ImGui::InvisibleButton("##Entry", ImVec2(width, line));
            ImDrawList&  drawList = *ImGui::GetWindowDrawList();
            const ImVec2 box0(pos.x + 2.0f, pos.y + 3.0f);
            const ImVec2 box1(pos.x + line - 3.0f, pos.y + line - 2.0f);
            if (visible)
                drawList.AddRectFilled(box0, box1, color);
            else
                drawList.AddRect(box0, box1, color);
            const ImU32 text = ImGui::GetColorU32(Theme::Resolve(visible ? Theme::TEXT : Theme::TEXT_MUTED));
            drawList.AddText(ImVec2(pos.x + line + 2.0f, pos.y), text, name);
            const std::string shown = FormatValue(value, count);
            drawList.AddText(ImVec2(pos.x + width - ImGui::CalcTextSize(shown.c_str()).x - 4.0f, pos.y), text, shown.c_str());
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(visible ? "Click to hide %s" : "Click to show %s", name);
            ImGui::PopID();
            return clicked;
        }

        double GetPassMs(std::span<const Renderer::GpuPassTiming> passes, const std::string& name)
        {
            for (const Renderer::GpuPassTiming& pass : passes)
            {
                if (pass.Name == name)
                    return pass.GpuMilliseconds;
            }
            return 0.0;
        }

        // A stable colour per zone name, by its text.
        ImU32 ZoneColor(std::string_view name)
        {
            uint32_t hash = 2166136261u;
            for (const char c : name)
                hash = (hash ^ static_cast<uint8_t>(c)) * 16777619u;
            float r = 0.0f, g = 0.0f, b = 0.0f;
            ImGui::ColorConvertHSVtoRGB(static_cast<float>(hash % 360) / 360.0f, 0.45f, 0.75f, r, g, b);
            return ImGui::GetColorU32(Theme::Resolve(ImVec4(r, g, b, 1.0f)));
        }

        // A bar with its name inside when it fits; whether the mouse is over it.
        bool DrawBarShape(ImDrawList& drawList, ImVec2 min, ImVec2 max, const char* name, ImU32 color, bool hovered)
        {
            max.x = std::max(max.x, min.x + 1.0f);
            drawList.AddRectFilled(min, max, color);
            if (max.x - min.x > 30.0f)
            {
                drawList.PushClipRect(min, max, true);
                const float textX = std::max(min.x, drawList.GetClipRectMin().x);
                drawList.AddText(ImVec2(textX + 3.0f, min.y + 1.0f), ImGui::GetColorU32(Theme::Resolve(Theme::MAIN_BG)), name);
                drawList.PopClipRect();
            }
            return hovered && ImGui::IsMouseHoveringRect(min, max);
        }

        // Coloured by its name, with a tooltip of the name and time.
        void DrawBar(ImDrawList& drawList, ImVec2 min, ImVec2 max, const char* name, double ms, bool hovered)
        {
            if (DrawBarShape(drawList, min, max, name, ZoneColor(name), hovered))
                ImGui::SetTooltip("%s\n%.3f ms", name, ms);
        }
    }

    ProfilerWindow::~ProfilerWindow()
    {
        if (m_Recording)
            Profiling::SetEnabled(false);
    }

    void ProfilerWindow::Draw(Renderer::Renderer* renderer)
    {
        if (!Open)
        {
            if (m_Recording)
            {
                Profiling::SetEnabled(false);
                if (renderer != nullptr)
                    renderer->SetCommandGpuTiming(false);
                m_Recording = false;
            }
            return;
        }
        if (!m_Recording)
        {
            ClearCapture();
            m_Paused    = false;
            m_Recording = true;
        }
        // GPU Usage and Rendering need every command timed, which costs: only while they are shown.
        const bool timeCommands = IsModuleShown(ProfilerModule::GpuUsage) || IsModuleShown(ProfilerModule::Rendering);
        Profiling::SetEnabled(!m_Paused);
        if (renderer != nullptr)
            renderer->SetCommandGpuTiming(timeCommands && !m_Paused);
        if (!m_Paused && renderer != nullptr)
            CollectGpu(*renderer);

        ImGui::SetNextWindowSize(ImVec2(900.0f, 680.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Profiler", &Open))
        {
            ImGui::End();
            return;
        }

        DrawToolbar();
        const size_t frameCount = Profiling::GetFrameCount();
        if (frameCount == 0)
        {
            ImGui::TextDisabled(m_Paused ? "Nothing recorded: press Record." : "Recording: the first frames appear next frame.");
            ImGui::End();
            return;
        }

        const size_t                  age   = GetShownAge();
        const Profiling::FrameRecord& frame = Profiling::GetFrame(age);
        const GpuSample*              gpu   = FindGpu(frame.Index);
        m_Shown                             = frame.Index;

        // The charts scroll on their own when the modules shown are taller than their share.
        if (std::find(m_ModuleShown.begin(), m_ModuleShown.end(), true) != m_ModuleShown.end())
        {
            if (ImGui::BeginChild("##ProfilerModules", ImVec2(0.0f, ImGui::GetContentRegionAvail().y * 0.42f)))
                DrawModules(age);
            ImGui::EndChild();
        }

        if (gpu != nullptr)
            ImGui::Text("CPU: %.3f ms   GPU: %.3f ms", GetFrameMs(frame), gpu->FrameMs);
        else
            ImGui::Text("CPU: %.3f ms   GPU: -", GetFrameMs(frame));

        const float timelineHeight = std::max(ImGui::GetContentRegionAvail().y * 0.55f, 120.0f);
        // Ctrl+wheel zooms the timeline, so it must not scroll it too.
        if (ImGui::BeginChild("##ProfilerTimeline", ImVec2(0.0f, timelineHeight), ImGuiChildFlags_Borders,
                              ImGui::GetIO().KeyCtrl ? ImGuiWindowFlags_NoScrollWithMouse : ImGuiWindowFlags_None))
            DrawTimeline(frame);
        ImGui::EndChild();
        if (ImGui::BeginTabBar("##ProfilerTables"))
        {
            if (ImGui::BeginTabItem("CPU zones"))
            {
                if (ImGui::BeginChild("##ProfilerTopZones", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
                    DrawTopZones(frame);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("GPU commands"))
            {
                if (ImGui::BeginChild("##ProfilerPassCommands", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
                    DrawPassCommands(gpu);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();
    }

    void ProfilerWindow::ClearCapture()
    {
        Profiling::Clear();
        m_Gpu.assign(m_Gpu.size(), GpuSample{});
        m_CpuCache.assign(m_CpuCache.size(), CpuSample{});
        m_GpuLegend.clear();
        m_Selected.reset();
        m_LastCollected = UINT64_MAX;
    }

    // The selected frame's age; unset, or once it has left the ring, the newest the GPU has timed
    // too (its timings arrive MAX_FRAMES_IN_FLIGHT frames after its CPU zones), else the newest.
    size_t ProfilerWindow::GetShownAge() const
    {
        const size_t   frameCount = Profiling::GetFrameCount();
        const uint64_t newest     = Profiling::GetFrame(0).Index;
        if (m_Selected && *m_Selected <= newest && newest - *m_Selected < frameCount)
            return static_cast<size_t>(newest - *m_Selected);
        for (size_t candidate = 0; candidate < std::min<size_t>(frameCount, 2 * HedgehogEngine::MAX_FRAMES_IN_FLIGHT + 1); ++candidate)
        {
            if (FindGpu(Profiling::GetFrame(candidate).Index) != nullptr)
                return candidate;
        }
        return 0;
    }

    // Unity's toolbar: the modules shown, Record, stepping through the kept frames, Clear and Save,
    // then the graph scale and the timeline's Fit.
    void ProfilerWindow::DrawToolbar()
    {
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("Profiler Modules").x + ImGui::GetFrameHeight() * 2.0f);
        if (ImGui::BeginCombo("##Modules", "Profiler Modules"))
        {
            for (size_t i = 0; i < PROFILER_MODULE_COUNT; ++i)
                ImGui::Checkbox(MODULE_NAMES[i], &m_ModuleShown[i]);
            ImGui::EndCombo();
        }
        if (!IsModuleShown(m_ActiveModule))
        {
            for (size_t i = 0; i < PROFILER_MODULE_COUNT; ++i)
            {
                if (m_ModuleShown[i])
                {
                    m_ActiveModule = static_cast<ProfilerModule>(i);
                    break;
                }
            }
        }

        ImGui::SameLine();
        if (RecordButton(!m_Paused))
            m_Paused = !m_Paused;

        // Previous and Next step through the kept frames; Current follows the newest again.
        const size_t frameCount = Profiling::GetFrameCount();
        ImGui::SameLine();
        ImGui::BeginDisabled(frameCount == 0);
        if (ImGui::ArrowButton("##PreviousFrame", ImGuiDir_Left))
        {
            const size_t age = GetShownAge();
            if (age + 1 < frameCount)
                m_Selected = Profiling::GetFrame(age + 1).Index;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Previous frame");
        ImGui::SameLine();
        if (ImGui::ArrowButton("##NextFrame", ImGuiDir_Right))
        {
            const size_t age = GetShownAge();
            if (age > 0)
                m_Selected = Profiling::GetFrame(age - 1).Index;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Next frame");
        ImGui::SameLine();
        if (ImGui::Button("Current"))
            m_Selected.reset();
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (frameCount > 0)
            ImGui::Text("Frame: %llu / %llu", static_cast<unsigned long long>(Profiling::GetFrame(GetShownAge()).Index),
                        static_cast<unsigned long long>(Profiling::GetFrame(0).Index));
        else
            ImGui::TextUnformatted("Frame: - / -");

        ImGui::SameLine();
        if (ImGui::Button("Clear"))
            ClearCapture();
        ImGui::SameLine();
        if (ImGui::Button("Save..."))
            Export();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Save the kept frames as a Chrome trace (Perfetto, chrome://tracing).");

        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("66.7 ms").x + ImGui::GetFrameHeight() * 2.0f);
        if (ImGui::BeginCombo("Scale", GRAPH_SCALES[m_GraphScale].Label))
        {
            for (int i = 0; i < static_cast<int>(std::size(GRAPH_SCALES)); ++i)
            {
                if (ImGui::Selectable(GRAPH_SCALES[i].Label, i == m_GraphScale))
                    m_GraphScale = i;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Fit"))
            m_View = TimelineView{};
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Show the whole frame. Ctrl+wheel over the timeline zooms; a right or middle drag pans.");
        if (!m_ExportStatus.empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", m_ExportStatus.c_str());
        }
    }

    // The shown modules, a row each: the module's name and legend on the left, its chart of the kept
    // frames on the right. A click in a chart selects that frame; clicking a name selects the module.
    void ProfilerWindow::DrawModules(size_t shownAge)
    {
        constexpr ImGuiTableFlags FLAGS = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersOuterH;
        if (!ImGui::BeginTable("##ProfilerModules", 2, FLAGS))
            return;
        ImGui::TableSetupColumn("Legend", ImGuiTableColumnFlags_WidthFixed, LEGEND_WIDTH);
        ImGui::TableSetupColumn("Chart", ImGuiTableColumnFlags_WidthStretch);

        const Profiling::FrameRecord& shown = Profiling::GetFrame(shownAge);
        for (size_t i = 0; i < PROFILER_MODULE_COUNT; ++i)
        {
            if (!m_ModuleShown[i])
                continue;
            const auto module = static_cast<ProfilerModule>(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Selectable(MODULE_NAMES[i], m_ActiveModule == module))
                m_ActiveModule = module;

            // The legend: each series with its colour and its value in the shown frame; a click hides
            // or shows it. The chart is at least as tall as the legend.
            m_Series.clear();
            CollectSeries(module, shown);
            for (Series& series : m_Series)
            {
                if (LegendEntry(series.Name, series.Color, series.Visible, series.Value, module == ProfilerModule::Rendering))
                    ToggleSeries(module, series);
            }
            const float legendEnd = ImGui::GetCursorPosY();
            ImGui::TableNextColumn();
            const float chartHeight = std::max(CHART_HEIGHT, legendEnd - ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);
            DrawChart(module, shownAge, chartHeight);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    // The series a module charts, in legend order, with their values in frame.
    void ProfilerWindow::CollectSeries(ProfilerModule module, const Profiling::FrameRecord& frame)
    {
        switch (module)
        {
        case ProfilerModule::CpuUsage:
        {
            const CategoryTimes& times = GetCpuTimes(frame);
            for (size_t c = 0; c < ZONE_CATEGORY_COUNT; ++c)
            {
                const auto category = static_cast<ZoneCategory>(c);
                m_Series.push_back({ GetZoneCategoryName(category), CategoryColor(category), !m_HiddenCategories[c], times[c] });
            }
            break;
        }
        case ProfilerModule::GpuUsage:
        {
            const GpuSample* gpu = FindGpu(frame.Index);
            for (const std::string& pass : m_GpuLegend)
            {
                const bool visible = std::find(m_HiddenPasses.begin(), m_HiddenPasses.end(), pass) == m_HiddenPasses.end();
                m_Series.push_back({ pass.c_str(), ZoneColor(pass), visible, gpu != nullptr ? GetPassMs(gpu->Passes, pass) : 0.0 });
            }
            break;
        }
        case ProfilerModule::Rendering:
        {
            const GpuSample* gpu = FindGpu(frame.Index);
            for (size_t c = 0; c < std::size(RENDERING_COUNTERS); ++c)
                m_Series.push_back({ RENDERING_COUNTERS[c], ImGui::GetColorU32(Theme::Resolve(Theme::FromHex(RENDERING_COLORS[c]))),
                                     !m_HiddenCounters[c], gpu != nullptr ? GetRenderingCounter(gpu->Commands, c) : 0.0 });
            break;
        }
        }
    }

    void ProfilerWindow::ToggleSeries(ProfilerModule module, const Series& series)
    {
        if (module == ProfilerModule::CpuUsage)
        {
            for (size_t c = 0; c < ZONE_CATEGORY_COUNT; ++c)
            {
                if (std::strcmp(GetZoneCategoryName(static_cast<ZoneCategory>(c)), series.Name) == 0)
                    m_HiddenCategories[c] = !m_HiddenCategories[c];
            }
        }
        else if (module == ProfilerModule::GpuUsage)
        {
            const auto hidden = std::find(m_HiddenPasses.begin(), m_HiddenPasses.end(), series.Name);
            if (hidden != m_HiddenPasses.end())
                m_HiddenPasses.erase(hidden);
            else
                m_HiddenPasses.emplace_back(series.Name);
        }
        else
        {
            for (size_t c = 0; c < std::size(RENDERING_COUNTERS); ++c)
            {
                if (std::strcmp(RENDERING_COUNTERS[c], series.Name) == 0)
                    m_HiddenCounters[c] = !m_HiddenCounters[c];
            }
        }
    }

    // A module's chart of the kept frames, oldest to newest: CPU and GPU time stacked by series up to
    // the scale with the 60 and 30 FPS lines, Rendering's counters as lines; the shown frame's cursor
    // over it, and a tooltip of the frame under the pointer.
    void ProfilerWindow::DrawChart(ProfilerModule module, size_t shownAge, float height)
    {
        const size_t frameCount = Profiling::GetFrameCount();
        const ImVec2 origin     = ImGui::GetCursorScreenPos();
        const float  width      = std::max(ImGui::GetContentRegionAvail().x, 50.0f);
        ImGui::InvisibleButton("##Chart", ImVec2(width, height));
        const bool  hovered  = ImGui::IsItemHovered();
        ImDrawList& drawList = *ImGui::GetWindowDrawList();
        drawList.AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), ImGui::GetColorU32(Theme::Resolve(CHART_BG)));
        drawList.PushClipRect(origin, ImVec2(origin.x + width, origin.y + height), true);

        // Every kept frame's series, oldest first, so the areas join left to right.
        const size_t series = [&]
        {
            m_Series.clear();
            CollectSeries(module, Profiling::GetFrame(0));
            return m_Series.size();
        }();
        m_ChartValues.assign(frameCount * series, 0.0);
        for (size_t age = 0; age < frameCount; ++age)
        {
            m_Series.clear();
            CollectSeries(module, Profiling::GetFrame(age));
            for (size_t s = 0; s < series && s < m_Series.size(); ++s)
                m_ChartValues[age * series + s] = m_Series[s].Visible ? m_Series[s].Value : 0.0;
        }
        m_Series.clear();
        CollectSeries(module, Profiling::GetFrame(0));

        const bool stacked = module != ProfilerModule::Rendering;
        double     top     = stacked ? GRAPH_SCALES[m_GraphScale].Ms : 0.0;
        if (top == 0.0)
        {
            // Auto: CPU time up to at least two budgets, as before; the GPU's (well under a
            // millisecond here) and the counters to what they reach.
            const bool cpu = module == ProfilerModule::CpuUsage;
            top            = cpu ? MIN_GRAPH_MS : stacked ? MIN_GPU_GRAPH_MS : 1.0;
            for (size_t age = 0; age < frameCount; ++age)
            {
                double sum = 0.0;
                for (size_t s = 0; s < series; ++s)
                    sum = stacked ? sum + m_ChartValues[age * series + s] : std::max(sum, m_ChartValues[age * series + s]);
                top = std::max(top, cpu ? sum : sum * 1.1);
            }
        }

        const float step   = width / static_cast<float>(Profiling::DEFAULT_FRAME_HISTORY);
        const float bottom = origin.y + height;
        const auto  xOf    = [&](size_t age) { return origin.x + width - (static_cast<float>(age) + 0.5f) * step; };
        const auto  yOf    = [&](double value) { return bottom - static_cast<float>(std::min(value / top, 1.0)) * height; };
        if (stacked)
        {
            // Each series' band between the sum of the ones below it and that plus its own, as one
            // quad between every two neighbouring frames.
            m_Below.assign(frameCount, 0.0);
            for (size_t s = 0; s < series; ++s)
            {
                if (!m_Series[s].Visible)
                    continue;
                for (size_t age = 0; age + 1 < frameCount; ++age)
                {
                    const double belowA = m_Below[age], belowB = m_Below[age + 1];
                    const double topA = belowA + m_ChartValues[age * series + s];
                    const double topB = belowB + m_ChartValues[(age + 1) * series + s];
                    drawList.AddQuadFilled(ImVec2(xOf(age), yOf(belowA)), ImVec2(xOf(age), yOf(topA)),
                                           ImVec2(xOf(age + 1), yOf(topB)), ImVec2(xOf(age + 1), yOf(belowB)), m_Series[s].Color);
                }
                for (size_t age = 0; age < frameCount; ++age)
                    m_Below[age] += m_ChartValues[age * series + s];
            }
            // The 60 and 30 FPS frame times, where the scale reaches them.
            const ImU32 lineColor = ImGui::GetColorU32(Theme::Resolve(Theme::TEXT_MUTED));
            for (const auto& [ms, label] : { std::pair{ FRAME_BUDGET_MS, "16ms (60FPS)" }, std::pair{ 2.0 * FRAME_BUDGET_MS, "33ms (30FPS)" } })
            {
                if (ms >= top)
                    continue;
                const float y = yOf(ms);
                drawList.AddLine(ImVec2(origin.x, y), ImVec2(origin.x + width, y), lineColor);
                drawList.AddText(ImVec2(origin.x + 4.0f, y - ImGui::GetTextLineHeight()), lineColor, label);
            }
        }
        else
        {
            for (size_t s = 0; s < series; ++s)
            {
                if (!m_Series[s].Visible)
                    continue;
                m_Points.clear();
                for (size_t age = 0; age < frameCount; ++age)
                    m_Points.emplace_back(xOf(age), yOf(m_ChartValues[age * series + s]));
                drawList.AddPolyline(m_Points.data(), static_cast<int>(m_Points.size()), m_Series[s].Color, ImDrawFlags_None, 1.5f);
            }
        }

        // The shown frame, as Unity's frame indicator.
        const float cursorX = xOf(shownAge);
        drawList.AddLine(ImVec2(cursorX, origin.y), ImVec2(cursorX, bottom), ImGui::GetColorU32(Theme::Resolve(Theme::TEXT)), 1.5f);
        drawList.PopClipRect();

        if (!hovered)
            return;
        const float offset = origin.x + width - ImGui::GetIO().MousePos.x;
        if (offset < 0.0f)
            return;
        const size_t age = static_cast<size_t>(offset / step);
        if (age >= frameCount)
            return;
        const Profiling::FrameRecord& frame = Profiling::GetFrame(age);
        m_Series.clear();
        CollectSeries(module, frame);
        std::string tooltip = "Frame " + std::to_string(frame.Index);
        for (const Series& entry : m_Series)
        {
            if (entry.Visible)
                tooltip += "\n" + std::string(entry.Name) + ": " + FormatValue(entry.Value, module == ProfilerModule::Rendering);
        }
        ImGui::SetTooltip("%s", tooltip.c_str());
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            m_Selected = frame.Index;
    }

    const CategoryTimes& ProfilerWindow::GetCpuTimes(const Profiling::FrameRecord& frame)
    {
        CpuSample& sample = m_CpuCache[frame.Index % m_CpuCache.size()];
        if (sample.FrameIndex != frame.Index)
        {
            sample.FrameIndex = frame.Index;
            sample.Times      = {};
            if (const Profiling::ThreadRecord* main = FindMainThread(frame))
                sample.Times = ComputeCategoryTimes(*main);
        }
        return sample.Times;
    }

    void ProfilerWindow::CollectGpu(const Renderer::Renderer& renderer)
    {
        if (Profiling::GetFrameCount() == 0)
            return;
        const uint64_t newest = Profiling::GetFrame(0).Index;
        if (newest == m_LastCollected || newest < HedgehogEngine::MAX_FRAMES_IN_FLIGHT)
            return;
        m_LastCollected = newest;
        const std::optional<double> frameMs = renderer.GetLastGpuFrameMs();
        if (!frameMs)
            return;

        // Kept by frame index, so a frame's sample is found at once.
        const uint64_t frameIndex = newest - HedgehogEngine::MAX_FRAMES_IN_FLIGHT;
        GpuSample&     sample     = m_Gpu[frameIndex % m_Gpu.size()];
        sample.FrameIndex         = frameIndex;
        sample.FrameMs    = *frameMs;
        const std::span<const Renderer::GpuPassTiming> passes = renderer.GetLastPassGpuTimings();
        sample.Passes.assign(passes.begin(), passes.end());
        const std::span<const Renderer::GpuCommandTiming> commands = renderer.GetLastCommandGpuTimings();
        sample.Commands.assign(commands.begin(), commands.end());
        sample.Valid = true;
        // The GPU Usage legend: every pass met, in the order first met.
        for (const Renderer::GpuPassTiming& pass : passes)
        {
            if (std::find(m_GpuLegend.begin(), m_GpuLegend.end(), pass.Name) == m_GpuLegend.end())
                m_GpuLegend.push_back(pass.Name);
        }
    }

    const ProfilerWindow::GpuSample* ProfilerWindow::FindGpu(uint64_t frameIndex) const
    {
        const GpuSample& sample = m_Gpu[frameIndex % m_Gpu.size()];
        return sample.Valid && sample.FrameIndex == frameIndex ? &sample : nullptr;
    }

    // Each thread's zones over the frame's span, a row per depth; then the GPU passes one after the
    // other on the same time scale.
    void ProfilerWindow::DrawTimeline(const Profiling::FrameRecord& frame)
    {
        const float  width    = std::max(ImGui::GetContentRegionAvail().x, 100.0f);
        const double frameMs  = std::max(GetFrameMs(frame), 1.0e-6);
        ImDrawList&  drawList = *ImGui::GetWindowDrawList();
        const bool   hovered  = ImGui::IsWindowHovered();
        const ImGuiIO& io     = ImGui::GetIO();
        const float  left     = ImGui::GetCursorScreenPos().x;

        // Ctrl+wheel zooms about the pointer; a right or middle drag begun here pans.
        if (hovered && io.KeyCtrl && io.MouseWheel != 0.0f)
            ZoomTimeline(m_View, (io.MousePos.x - left) / width, std::pow(ZOOM_PER_NOTCH, io.MouseWheel), frameMs * 1.0e6);
        if (hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle)))
            m_Panning = true;
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) && !ImGui::IsMouseDown(ImGuiMouseButton_Middle))
            m_Panning = false;
        if (m_Panning && io.MouseDelta.x != 0.0f)
            PanTimeline(m_View, -io.MouseDelta.x / width);

        // The frame laid out over fullWidth, of which the view's part is shown from left.
        const float fullWidth = width / static_cast<float>(m_View.Span());
        const float shift     = left - static_cast<float>(m_View.Begin) * fullWidth;
        DrawRuler(frameMs, width);

        for (const Profiling::ThreadRecord& thread : frame.Threads)
        {
            ImGui::TextUnformatted(thread.ThreadName.c_str());
            LayoutTimeline(thread, frame.StartNs, frame.EndNs, fullWidth, m_Bars);
            uint32_t rows = 0;
            for (const TimelineBar& bar : m_Bars)
                rows = std::max(rows, bar.Row + 1);
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(width, static_cast<float>(rows) * ROW_HEIGHT));
            for (const TimelineBar& bar : m_Bars)
            {
                if (shift + bar.X1 < left || shift + bar.X0 > left + width)
                    continue;
                const float y = origin.y + static_cast<float>(bar.Row) * ROW_HEIGHT;
                DrawBar(drawList, ImVec2(shift + bar.X0, y), ImVec2(shift + bar.X1, y + ROW_HEIGHT - 2.0f),
                        bar.Name, bar.Ms, hovered);
            }
        }

        ImGui::TextUnformatted("GPU");
        const GpuSample* gpu = FindGpu(frame.Index);
        if (gpu == nullptr)
        {
            ImGui::TextDisabled("No GPU timings for this frame.");
            return;
        }
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(width, ROW_HEIGHT));
        float x = shift;
        for (const Renderer::GpuPassTiming& pass : gpu->Passes)
        {
            const float w = static_cast<float>(pass.GpuMilliseconds / frameMs) * fullWidth;
            DrawBar(drawList, ImVec2(x, origin.y), ImVec2(x + w, origin.y + ROW_HEIGHT - 2.0f), pass.Name.c_str(),
                    pass.GpuMilliseconds, hovered);
            x += w;
        }

        // Each command one after another on the same scale, coloured as its pass.
        if (gpu->Commands.empty())
            return;
        ImGui::TextUnformatted("GPU commands");
        const ImVec2 commandOrigin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(width, ROW_HEIGHT));
        x = shift;
        for (const Renderer::GpuCommandTiming& command : gpu->Commands)
        {
            const float w = static_cast<float>(command.GpuMilliseconds / frameMs) * fullWidth;
            if (x + w < left || x > left + width)
            {
                x += w;
                continue;
            }
            if (DrawBarShape(drawList, ImVec2(x, commandOrigin.y), ImVec2(x + w, commandOrigin.y + ROW_HEIGHT - 2.0f),
                             Renderer::GetGpuCommandKindName(command.Kind), ZoneColor(command.Pass), hovered))
                ImGui::SetTooltip("%s\n%s\n%.4f ms", DescribeGpuCommand(command).c_str(), command.Pass,
                                  command.GpuMilliseconds);
            x += w;
        }
    }

    // The view's span of the frame with a tick, its time from the frame's start, every step.
    void ProfilerWindow::DrawRuler(double frameMs, float width)
    {
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(width, RULER_HEIGHT));
        ImDrawList& drawList = *ImGui::GetWindowDrawList();
        const ImU32 color    = ImGui::GetColorU32(Theme::Resolve(Theme::TEXT_MUTED));
        drawList.AddLine(ImVec2(origin.x, origin.y + RULER_HEIGHT - 1.0f), ImVec2(origin.x + width, origin.y + RULER_HEIGHT - 1.0f), color);

        const double beginMs = m_View.Begin * frameMs;
        const double spanMs  = m_View.Span() * frameMs;
        const double step    = ChooseTickStep(spanMs, width, MIN_TICK_PIXELS);
        for (double tick = std::ceil(beginMs / step) * step; tick <= beginMs + spanMs; tick += step)
        {
            const float x = origin.x + static_cast<float>((tick - beginMs) / spanMs) * width;
            drawList.AddLine(ImVec2(x, origin.y + RULER_HEIGHT - 6.0f), ImVec2(x, origin.y + RULER_HEIGHT - 1.0f), color);
            drawList.AddText(ImVec2(x + 3.0f, origin.y), color, FormatTickLabel(tick, step).c_str());
        }
    }

    void ProfilerWindow::Export()
    {
        const char* path = DialogueWindows::ProfilerTraceSaveDialogue("profile.json");
        if (path == nullptr)
            return;

        // Oldest first; each frame's GPU passes start where its CPU frame does (see TraceGpuFrame).
        std::vector<const Profiling::FrameRecord*> frames;
        std::vector<TraceGpuFrame>                 gpuFrames;
        for (size_t age = Profiling::GetFrameCount(); age > 0; --age)
        {
            const Profiling::FrameRecord& frame = Profiling::GetFrame(age - 1);
            frames.push_back(&frame);
            if (const GpuSample* gpu = FindGpu(frame.Index))
                gpuFrames.push_back({ frame.StartNs, gpu->Passes, gpu->Commands });
        }

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << WriteChromeTrace(frames, gpuFrames);
        m_ExportStatus = file ? "Exported " + std::to_string(frames.size()) + " frames to " + path
                              : std::string("Could not write ") + path;
    }

    void ProfilerWindow::DrawTopZones(const Profiling::FrameRecord& frame)
    {
        ComputeTopZones(frame, m_TopZones);
        constexpr ImGuiTableFlags FLAGS = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                          ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
        if (!ImGui::BeginTable("##TopZones", 4, FLAGS))
            return;
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Zone", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("Total ms", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Self ms", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_WidthStretch, 0.7f);
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < std::min(m_TopZones.size(), TOP_ZONE_ROWS); ++i)
        {
            const ZoneStat& stat = m_TopZones[i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(stat.Name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%.3f", stat.TotalMs);
            ImGui::TableNextColumn();
            ImGui::Text("%.3f", stat.SelfMs);
            ImGui::TableNextColumn();
            ImGui::Text("%u", stat.Calls);
        }
        ImGui::EndTable();
    }

    // The timed commands by pass: how many, how many draws and their GPU time.
    void ProfilerWindow::DrawPassCommands(const GpuSample* gpu)
    {
        if (!IsModuleShown(ProfilerModule::GpuUsage) && !IsModuleShown(ProfilerModule::Rendering))
        {
            ImGui::TextDisabled("Add the GPU Usage module (Profiler Modules) to time every draw, barrier, copy and blit.");
            return;
        }
        if (gpu == nullptr || gpu->Commands.empty())
        {
            ImGui::TextDisabled("No timed GPU commands for this frame yet.");
            return;
        }
        SummarizeCommandsByPass(gpu->Commands, m_PassCommands);
        constexpr ImGuiTableFlags FLAGS = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                          ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
        if (!ImGui::BeginTable("##PassCommands", 4, FLAGS))
            return;
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("Commands", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Draws", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("GPU ms", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableHeadersRow();
        for (const PassCommandStat& stat : m_PassCommands)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(stat.Pass.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%u", stat.Commands);
            ImGui::TableNextColumn();
            ImGui::Text("%u", stat.Draws);
            ImGui::TableNextColumn();
            ImGui::Text("%.3f", stat.GpuMs);
        }
        ImGui::EndTable();
    }
}

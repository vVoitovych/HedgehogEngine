#include "ProfilerWindow.hpp"

#include "EditorTheme.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogRenderer/Renderer.hpp"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <span>
#include <string_view>

namespace Editor
{
    namespace
    {
        constexpr float  GRAPH_HEIGHT    = 90.0f;
        constexpr float  ROW_HEIGHT      = 18.0f;
        constexpr double FRAME_BUDGET_MS = 1000.0 / 60.0;
        constexpr double MIN_GRAPH_MS    = 2.0 * FRAME_BUDGET_MS;
        constexpr size_t TOP_ZONE_ROWS   = 60;

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

        // A bar with its name inside when it fits, and a tooltip of the name and time when hovered.
        void DrawBar(ImDrawList& drawList, ImVec2 min, ImVec2 max, const char* name, double ms, bool hovered)
        {
            max.x = std::max(max.x, min.x + 1.0f);
            drawList.AddRectFilled(min, max, ZoneColor(name));
            if (max.x - min.x > 30.0f)
            {
                drawList.PushClipRect(min, max, true);
                drawList.AddText(ImVec2(min.x + 3.0f, min.y + 1.0f), ImGui::GetColorU32(Theme::Resolve(Theme::MAIN_BG)), name);
                drawList.PopClipRect();
            }
            if (hovered && ImGui::IsMouseHoveringRect(min, max))
                ImGui::SetTooltip("%s\n%.3f ms", name, ms);
        }
    }

    ProfilerWindow::~ProfilerWindow()
    {
        if (m_Recording)
            Profiling::SetEnabled(false);
    }

    void ProfilerWindow::Draw(const Renderer::Renderer* renderer)
    {
        if (!Open)
        {
            if (m_Recording)
            {
                Profiling::SetEnabled(false);
                m_Recording = false;
            }
            return;
        }
        if (!m_Recording)
        {
            Profiling::Clear();
            m_Gpu.assign(m_Gpu.size(), GpuSample{});
            m_Selected.reset();
            m_Paused    = false;
            m_Recording = true;
        }
        Profiling::SetEnabled(!m_Paused);
        if (!m_Paused && renderer != nullptr)
            CollectGpu(*renderer);

        ImGui::SetNextWindowSize(ImVec2(900.0f, 620.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Profiler", &Open))
        {
            ImGui::End();
            return;
        }

        if (ImGui::Button(m_Paused ? "Resume" : "Pause"))
        {
            m_Paused = !m_Paused;
            if (!m_Paused)
                m_Selected.reset(); // follow the newest frame again
        }
        ImGui::SameLine();
        const size_t frameCount = Profiling::GetFrameCount();
        if (frameCount == 0)
        {
            ImGui::TextDisabled("Recording: the first frames appear next frame.");
            ImGui::End();
            return;
        }

        // The selected frame; unset, or once it has left the ring, the newest the GPU has timed too
        // (its timings arrive MAX_FRAMES_IN_FLIGHT frames after its CPU zones), else the newest.
        const uint64_t newest = Profiling::GetFrame(0).Index;
        size_t         age    = 0;
        if (m_Selected && *m_Selected <= newest && newest - *m_Selected < frameCount)
            age = static_cast<size_t>(newest - *m_Selected);
        else
        {
            for (size_t candidate = 0; candidate < std::min<size_t>(frameCount, 2 * HedgehogEngine::MAX_FRAMES_IN_FLIGHT + 1); ++candidate)
            {
                if (FindGpu(Profiling::GetFrame(candidate).Index) != nullptr)
                {
                    age = candidate;
                    break;
                }
            }
        }
        const Profiling::FrameRecord& frame = Profiling::GetFrame(age);
        const GpuSample*              gpu   = FindGpu(frame.Index);
        m_Shown                             = frame.Index;
        if (gpu != nullptr)
            ImGui::Text("Frame %llu: %.3f ms on the CPU, %.3f ms on the GPU", static_cast<unsigned long long>(frame.Index),
                        GetFrameMs(frame), gpu->FrameMs);
        else
            ImGui::Text("Frame %llu: %.3f ms on the CPU", static_cast<unsigned long long>(frame.Index), GetFrameMs(frame));
        ImGui::SameLine();
        ImGui::TextDisabled("(click a bar to inspect its frame; it pauses)");

        DrawFrameGraph();

        const float timelineHeight = std::max(ImGui::GetContentRegionAvail().y * 0.55f, 120.0f);
        if (ImGui::BeginChild("##ProfilerTimeline", ImVec2(0.0f, timelineHeight), ImGuiChildFlags_Borders,
                              ImGuiWindowFlags_HorizontalScrollbar))
            DrawTimeline(frame);
        ImGui::EndChild();
        if (ImGui::BeginChild("##ProfilerTopZones", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
            DrawTopZones(frame);
        ImGui::EndChild();
        ImGui::End();
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

        GpuSample& sample = m_Gpu[m_GpuNext];
        m_GpuNext         = (m_GpuNext + 1) % m_Gpu.size();
        sample.FrameIndex = newest - HedgehogEngine::MAX_FRAMES_IN_FLIGHT;
        sample.FrameMs    = *frameMs;
        const std::span<const Renderer::GpuPassTiming> passes = renderer.GetLastPassGpuTimings();
        sample.Passes.assign(passes.begin(), passes.end());
        sample.Valid = true;
    }

    const ProfilerWindow::GpuSample* ProfilerWindow::FindGpu(uint64_t frameIndex) const
    {
        for (const GpuSample& sample : m_Gpu)
        {
            if (sample.Valid && sample.FrameIndex == frameIndex)
                return &sample;
        }
        return nullptr;
    }

    // The kept frames oldest to newest: CPU bars, a thinner GPU bar over each, and the budget line.
    void ProfilerWindow::DrawFrameGraph()
    {
        const size_t frameCount = Profiling::GetFrameCount();
        const ImVec2 origin     = ImGui::GetCursorScreenPos();
        const float  width      = std::max(ImGui::GetContentRegionAvail().x, 50.0f);
        ImGui::InvisibleButton("##ProfilerGraph", ImVec2(width, GRAPH_HEIGHT));
        const bool hovered = ImGui::IsItemHovered();

        double scaleMs = MIN_GRAPH_MS;
        for (size_t age = 0; age < frameCount; ++age)
            scaleMs = std::max(scaleMs, GetFrameMs(Profiling::GetFrame(age)));

        ImDrawList& drawList = *ImGui::GetWindowDrawList();
        drawList.AddRectFilled(origin, ImVec2(origin.x + width, origin.y + GRAPH_HEIGHT),
                               ImGui::GetColorU32(ImGuiCol_FrameBg));
        const float step = width / static_cast<float>(Profiling::DEFAULT_FRAME_HISTORY);
        const auto  barHeight = [&](double ms) { return static_cast<float>(std::min(ms / scaleMs, 1.0)) * GRAPH_HEIGHT; };
        const ImU32 cpuColor  = ImGui::GetColorU32(Theme::Resolve(Theme::ACCENT));
        const ImU32 gpuColor  = ImGui::GetColorU32(Theme::Resolve(Theme::PLAY_TINT));
        const ImU32 pickColor = ImGui::GetColorU32(Theme::Resolve(Theme::TEXT));

        for (size_t age = 0; age < frameCount; ++age)
        {
            const Profiling::FrameRecord& frame = Profiling::GetFrame(age);
            const float  x0  = origin.x + width - static_cast<float>(age + 1) * step;
            const float  x1  = x0 + std::max(step - 1.0f, 1.0f);
            const float  bottom = origin.y + GRAPH_HEIGHT;
            const double cpu = GetFrameMs(frame);
            const bool   selected = frame.Index == m_Shown;
            drawList.AddRectFilled(ImVec2(x0, bottom - barHeight(cpu)), ImVec2(x1, bottom), selected ? pickColor : cpuColor);
            const GpuSample* gpu = FindGpu(frame.Index);
            if (gpu != nullptr)
            {
                const float inset = (x1 - x0) * 0.3f;
                drawList.AddRectFilled(ImVec2(x0 + inset, bottom - barHeight(gpu->FrameMs)), ImVec2(x1 - inset, bottom), gpuColor);
            }

            if (hovered && ImGui::GetIO().MousePos.x >= x0 && ImGui::GetIO().MousePos.x < x0 + step)
            {
                if (gpu != nullptr)
                    ImGui::SetTooltip("Frame %llu\nCPU %.3f ms\nGPU %.3f ms", static_cast<unsigned long long>(frame.Index), cpu, gpu->FrameMs);
                else
                    ImGui::SetTooltip("Frame %llu\nCPU %.3f ms", static_cast<unsigned long long>(frame.Index), cpu);
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    m_Selected = frame.Index;
                    m_Paused   = true;
                }
            }
        }

        // The 60 FPS budget.
        const float budgetY = origin.y + GRAPH_HEIGHT - barHeight(FRAME_BUDGET_MS);
        drawList.AddLine(ImVec2(origin.x, budgetY), ImVec2(origin.x + width, budgetY),
                         ImGui::GetColorU32(Theme::Resolve(Theme::TEXT_MUTED)));
        drawList.AddText(ImVec2(origin.x + 4.0f, budgetY - ImGui::GetTextLineHeight()),
                         ImGui::GetColorU32(Theme::Resolve(Theme::TEXT_MUTED)), "16.6 ms");
    }

    // Each thread's zones over the frame's span, a row per depth; then the GPU passes one after the
    // other on the same time scale.
    void ProfilerWindow::DrawTimeline(const Profiling::FrameRecord& frame)
    {
        const float  width    = std::max(ImGui::GetContentRegionAvail().x, 100.0f);
        const double frameMs  = std::max(GetFrameMs(frame), 1.0e-6);
        ImDrawList&  drawList = *ImGui::GetWindowDrawList();
        const bool   hovered  = ImGui::IsWindowHovered();

        for (const Profiling::ThreadRecord& thread : frame.Threads)
        {
            ImGui::TextUnformatted(thread.ThreadName.c_str());
            LayoutTimeline(thread, frame.StartNs, frame.EndNs, width, m_Bars);
            uint32_t rows = 0;
            for (const TimelineBar& bar : m_Bars)
                rows = std::max(rows, bar.Row + 1);
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(width, static_cast<float>(rows) * ROW_HEIGHT));
            for (const TimelineBar& bar : m_Bars)
            {
                const float y = origin.y + static_cast<float>(bar.Row) * ROW_HEIGHT;
                DrawBar(drawList, ImVec2(origin.x + bar.X0, y), ImVec2(origin.x + bar.X1, y + ROW_HEIGHT - 2.0f),
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
        float x = origin.x;
        for (const Renderer::GpuPassTiming& pass : gpu->Passes)
        {
            const float w = static_cast<float>(pass.GpuMilliseconds / frameMs) * width;
            DrawBar(drawList, ImVec2(x, origin.y), ImVec2(x + w, origin.y + ROW_HEIGHT - 2.0f), pass.Name.c_str(),
                    pass.GpuMilliseconds, hovered);
            x += w;
        }
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
}

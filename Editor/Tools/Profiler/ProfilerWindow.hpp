#pragma once

#include "ProfilerAnalysis.hpp"
#include "ProfilerHierarchy.hpp"
#include "TimelineView.hpp"

#include "HedgehogRenderer/Graph/GpuPassTiming.hpp"

#include "imgui.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Renderer
{
    class Renderer;
}

namespace Editor
{
    // The Profiler's modules, as Unity's: each a chart of the kept frames and a legend.
    enum class ProfilerModule : uint8_t
    {
        CpuUsage,  // the main thread's time by ZoneCategory
        GpuUsage,  // the GPU's time by render-graph pass; times every GPU command while shown
        Rendering, // draw calls and commands; likewise
    };
    inline constexpr size_t PROFILER_MODULE_COUNT = 3;

    // Window > Profiler, laid out as Unity's: a toolbar (Profiler Modules, Record, previous, next and
    // current frame, Clear, Save), the shown modules' charts of the kept frames with their legends
    // and a frame cursor, then the shown frame's timeline and tables. The profiler records only while
    // the window is open; Record off stops it, so the frames kept hold still for a look.
    class ProfilerWindow
    {
    public:
        bool Open = false;

        ProfilerWindow() = default;
        ~ProfilerWindow(); // stops the recording it started (the editor is rebuilt on a project switch)
        ProfilerWindow(const ProfilerWindow&)            = delete;
        ProfilerWindow& operator=(const ProfilerWindow&) = delete;

        // renderer may be null (no GPU timings); the window turns its command timing on and off.
        void Draw(Renderer::Renderer* renderer);

    private:
        // The GPU timings the renderer reported, for the frame they belong to (MAX_FRAMES_IN_FLIGHT
        // behind the newest recorded one, the frame the GPU last finished).
        struct GpuSample
        {
            uint64_t                             FrameIndex = 0;
            double                               FrameMs    = 0.0;
            std::vector<Renderer::GpuPassTiming> Passes;
            std::vector<Renderer::GpuCommandTiming> Commands; // empty unless commands were timed
            bool                                 Valid      = false;
        };

        // One line of a module's legend and one series of its chart.
        struct Series
        {
            const char* Name    = "";
            uint32_t    Color   = 0; // ImU32
            bool        Visible = true;
            double      Value   = 0.0; // in the frame asked for
        };

        // A kept frame's CPU Usage, worked out once per frame.
        struct CpuSample
        {
            uint64_t      FrameIndex = UINT64_MAX;
            CategoryTimes Times{};
        };

        void             CollectGpu(const Renderer::Renderer& renderer);
        const GpuSample* FindGpu(uint64_t frameIndex) const;
        void             ClearCapture();
        size_t           GetShownAge() const;
        void             DrawToolbar();
        void             DrawModules(size_t shownAge);
        void             CollectSeries(ProfilerModule module, const Profiling::FrameRecord& frame);
        void             ToggleSeries(ProfilerModule module, const Series& series);
        void             DrawChart(ProfilerModule module, size_t shownAge, float height);
        const CategoryTimes& GetCpuTimes(const Profiling::FrameRecord& frame);
        bool             IsModuleShown(ProfilerModule module) const { return m_ModuleShown[static_cast<size_t>(module)]; }
        void             DrawTimeline(const Profiling::FrameRecord& frame);
        void             DrawRuler(double frameMs, float width);
        // The details pane under the charts (ProfilerDetails.cpp).
        void             DrawDetails(const Profiling::FrameRecord& frame, const GpuSample* gpu);
        void             DrawSelectedSample(const Profiling::FrameRecord& frame);
        void             DrawHierarchy(const Profiling::FrameRecord& frame);
        void             DrawTreeRow(uint32_t node, bool tree);
        void             DrawPassCommands(const GpuSample* gpu);
        // Asks where, then writes every frame kept with its GPU passes as Chrome trace JSON.
        void             Export();

        bool                    m_Recording = false; // the window turned the profiler on
        bool                    m_Paused    = false; // Record off
        TimelineView            m_View;                 // the part of the shown frame the timeline shows
        bool                    m_Panning      = false; // a right or middle drag begun over the timeline
        int                     m_GraphScale   = 0;     // GRAPH_SCALES: Auto, or a fixed top of the time charts
        std::array<bool, PROFILER_MODULE_COUNT> m_ModuleShown{ true, true, false };
        ProfilerModule          m_ActiveModule = ProfilerModule::CpuUsage; // the one whose name is selected
        std::array<bool, ZONE_CATEGORY_COUNT> m_HiddenCategories{};
        std::vector<std::string> m_HiddenPasses;
        std::array<bool, 2>     m_HiddenCounters{};
        std::vector<std::string> m_GpuLegend; // every pass met since the capture began

        // The details pane: CPU Usage's view, the thread its Hierarchy views show, the search and
        // the sample picked on the timeline (kept while its frame is shown).
        enum class DetailsView : uint8_t
        {
            Timeline,
            Hierarchy,
            InvertedHierarchy,
        };
        struct SelectedSample
        {
            uint64_t    FrameIndex  = UINT64_MAX;
            uint32_t    ThreadIndex = 0;
            uint64_t    StartNs     = 0;
            const char* Name        = nullptr;
        };
        DetailsView           m_DetailsView = DetailsView::Timeline;
        std::string           m_DetailsThread = "Main";
        char                  m_Search[128]   = {};
        SelectedSample        m_SelectedSample;
        ProfilerTree          m_Tree;      // rebuilt every frame a Hierarchy view shows
        std::vector<uint32_t> m_TreeStack; // scratch
        std::optional<uint64_t> m_Selected;          // a frame's Index; the newest when unset
        uint64_t                m_LastCollected = UINT64_MAX;
        uint64_t                m_Shown         = 0;   // the frame the timeline shows, highlighted in the graph

        std::vector<GpuSample>   m_Gpu = std::vector<GpuSample>(Profiling::DEFAULT_FRAME_HISTORY);
        std::vector<CpuSample>   m_CpuCache = std::vector<CpuSample>(Profiling::DEFAULT_FRAME_HISTORY);
        std::vector<Series>      m_Series;      // scratch
        std::vector<double>      m_ChartValues; // scratch: every kept frame's series
        std::vector<double>      m_Below;       // scratch: the stack under a series
        std::vector<ImVec2>      m_Points;      // scratch: a counter's line
        std::vector<ZoneStat>    m_TopZones;
        std::vector<TimelineBar> m_Bars;
        std::vector<PassCommandStat> m_PassCommands;
        std::string              m_ExportStatus; // what the last export did
    };
}

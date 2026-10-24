#pragma once

#include "ProfilerAnalysis.hpp"

#include "HedgehogRenderer/Graph/GpuPassTiming.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace Renderer
{
    class Renderer;
}

namespace Editor
{
    // Window > Profiler: the CPU zones of recent frames and the GPU time of their passes. The
    // profiler records only while the window is open; Pause stops it, so the frames kept hold
    // still for a look.
    class ProfilerWindow
    {
    public:
        bool Open = false;

        ProfilerWindow() = default;
        ~ProfilerWindow(); // stops the recording it started (the editor is rebuilt on a project switch)
        ProfilerWindow(const ProfilerWindow&)            = delete;
        ProfilerWindow& operator=(const ProfilerWindow&) = delete;

        // renderer may be null (no GPU timings).
        void Draw(const Renderer::Renderer* renderer);

    private:
        // The GPU timings the renderer reported, for the frame they belong to (MAX_FRAMES_IN_FLIGHT
        // behind the newest recorded one, the frame the GPU last finished).
        struct GpuSample
        {
            uint64_t                             FrameIndex = 0;
            double                               FrameMs    = 0.0;
            std::vector<Renderer::GpuPassTiming> Passes;
            bool                                 Valid      = false;
        };

        void             CollectGpu(const Renderer::Renderer& renderer);
        const GpuSample* FindGpu(uint64_t frameIndex) const;
        void             DrawFrameGraph();
        void             DrawTimeline(const Profiling::FrameRecord& frame);
        void             DrawTopZones(const Profiling::FrameRecord& frame);

        bool                    m_Recording = false; // the window turned the profiler on
        bool                    m_Paused    = false;
        std::optional<uint64_t> m_Selected;          // a frame's Index; the newest when unset
        uint64_t                m_LastCollected = UINT64_MAX;
        uint64_t                m_Shown         = 0;   // the frame the timeline shows, highlighted in the graph

        std::vector<GpuSample>   m_Gpu = std::vector<GpuSample>(Profiling::DEFAULT_FRAME_HISTORY);
        size_t                   m_GpuNext = 0;
        std::vector<ZoneStat>    m_TopZones;
        std::vector<TimelineBar> m_Bars;
    };
}

#pragma once

#include "Profiler/api/Profiler.hpp"

#include "HedgehogRenderer/Graph/GpuPassTiming.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// What the Profiler window shows of a recorded frame, worked out without ImGui so EditorTest
// covers it: the zones' totals and self times, and where each zone's bar goes on the timeline.
namespace Editor
{
    // One zone name's share of a frame, over every thread.
    struct ZoneStat
    {
        std::string Name;
        double      TotalMs = 0.0; // its zones' whole durations
        double      SelfMs  = 0.0; // minus the time inside the zones nested in them
        uint32_t    Calls   = 0;
    };

    // Every zone name of the frame, by total time, the longest first (ties by name). Names compare
    // by text: one name made in two modules is one entry.
    void ComputeTopZones(const Profiling::FrameRecord& frame, std::vector<ZoneStat>& out);

    // A zone's bar on its thread's timeline: from X0 to X1 in [0, width] of the frame's span (a
    // zone begun before the frame starts at 0), on row Depth.
    struct TimelineBar
    {
        float       X0    = 0.0f;
        float       X1    = 0.0f;
        uint32_t    Row   = 0;
        const char* Name  = nullptr;
        double      Ms    = 0.0;
    };

    // The thread's zones laid over a frame of the given span, in start order.
    void LayoutTimeline(const Profiling::ThreadRecord& thread, uint64_t frameStartNs, uint64_t frameEndNs, float width,
                        std::vector<TimelineBar>& out);

    // The thread's zones by start, a parent before the zones nested in it.
    [[nodiscard]] std::vector<const Profiling::ZoneRecord*> SortZonesByStart(const Profiling::ThreadRecord& thread);

    // The frame's length in milliseconds.
    [[nodiscard]] double GetFrameMs(const Profiling::FrameRecord& frame);

    // A GPU command as the window names it: its kind, its count in the kind's unit and, for more
    // than one, its instances ("DrawIndexed 36 indices x 2 instances", "Barrier 1 transition").
    [[nodiscard]] std::string DescribeGpuCommand(const Renderer::GpuCommandTiming& command);

    // One pass's share of the timed GPU commands.
    struct PassCommandStat
    {
        std::string Pass;
        uint32_t    Commands = 0;
        uint32_t    Draws    = 0; // Draw and DrawIndexed
        double      GpuMs    = 0.0;
    };

    // The commands summed by pass, in the order the passes first appear.
    void SummarizeCommandsByPass(std::span<const Renderer::GpuCommandTiming> commands, std::vector<PassCommandStat>& out);

    // What a zone's time is spent on, as the CPU Usage chart stacks it (Unity's categories, ours
    // where the engine differs: Waiting for GPU in place of VSync).
    enum class ZoneCategory : uint8_t
    {
        Rendering,
        Scripts,
        Physics,
        Animation,
        UI,
        WaitingForGpu,
        Others,
    };
    inline constexpr size_t ZONE_CATEGORY_COUNT = 7;

    [[nodiscard]] const char* GetZoneCategoryName(ZoneCategory category);
    // The category's chart colour, 0xRRGGBB in sRGB.
    [[nodiscard]] uint32_t GetZoneCategoryColor(ZoneCategory category);

    // The category a zone's name puts it in, or none for a zone whose time belongs to its parent's
    // category: Rendering (Render, RenderFrame, SyncResources, Extract: ...), Scripts (Scripts: ...,
    // ScriptSystem), Physics (Physics: ..., PhysicsSystem), Animation (Phase: Animation,
    // AnimationSystem), UI (Editor UI, Outside windows, UiSystem), Waiting for GPU (Wait for the GPU).
    [[nodiscard]] std::optional<ZoneCategory> CategorizeZone(std::string_view name);

    // Milliseconds per category, indexed by ZoneCategory.
    using CategoryTimes = std::array<double, ZONE_CATEGORY_COUNT>;

    // The thread's time split by category: each zone's self time (its duration minus the zones
    // directly nested in it) goes to its own category, else its nearest categorized ancestor's,
    // else Others, so the categories sum to the thread's top-level zones.
    [[nodiscard]] CategoryTimes ComputeCategoryTimes(const Profiling::ThreadRecord& thread);
}

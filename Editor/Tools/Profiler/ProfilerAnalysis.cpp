#include "ProfilerAnalysis.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Editor
{
    namespace
    {
        constexpr double NS_PER_MS = 1.0e6;
    }

    std::vector<const Profiling::ZoneRecord*> SortZonesByStart(const Profiling::ThreadRecord& thread)
    {
        std::vector<const Profiling::ZoneRecord*> zones;
        zones.reserve(thread.Zones.size());
        for (const Profiling::ZoneRecord& zone : thread.Zones)
            zones.push_back(&zone);
        std::stable_sort(zones.begin(), zones.end(), [](const Profiling::ZoneRecord* a, const Profiling::ZoneRecord* b)
        {
            return a->StartNs != b->StartNs ? a->StartNs < b->StartNs : a->Depth < b->Depth;
        });
        return zones;
    }

    void ComputeTopZones(const Profiling::FrameRecord& frame, std::vector<ZoneStat>& out)
    {
        out.clear();
        // An index, not a reference: adding a name may move the others.
        const auto statFor = [&](const char* name) -> size_t
        {
            for (size_t i = 0; i < out.size(); ++i)
            {
                if (out[i].Name == name)
                    return i;
            }
            out.push_back({ name });
            return out.size() - 1;
        };

        for (const Profiling::ThreadRecord& thread : frame.Threads)
        {
            // Walking zones in start order with a stack of the open ones: each zone's parent is the
            // innermost one still open, whose self time loses the zone's duration.
            std::vector<std::pair<const Profiling::ZoneRecord*, size_t>> open;
            for (const Profiling::ZoneRecord* zone : SortZonesByStart(thread))
            {
                while (!open.empty() && open.back().first->Depth >= zone->Depth)
                    open.pop_back();
                const double ms    = static_cast<double>(zone->DurationNs) / NS_PER_MS;
                const size_t index = statFor(zone->Name);
                out[index].TotalMs += ms;
                out[index].SelfMs  += ms;
                ++out[index].Calls;
                if (!open.empty())
                    out[open.back().second].SelfMs -= ms;
                open.emplace_back(zone, index);
            }
        }

        std::sort(out.begin(), out.end(), [](const ZoneStat& a, const ZoneStat& b)
        {
            return a.TotalMs != b.TotalMs ? a.TotalMs > b.TotalMs : a.Name < b.Name;
        });
    }

    void LayoutTimeline(const Profiling::ThreadRecord& thread, uint64_t frameStartNs, uint64_t frameEndNs, float width,
                        std::vector<TimelineBar>& out)
    {
        out.clear();
        const double span = static_cast<double>(std::max<uint64_t>(frameEndNs - frameStartNs, 1));
        const auto   toX  = [&](uint64_t ns)
        {
            const double offset = ns <= frameStartNs ? 0.0 : static_cast<double>(ns - frameStartNs);
            return static_cast<float>(std::min(offset / span, 1.0)) * width;
        };
        for (const Profiling::ZoneRecord* zone : SortZonesByStart(thread))
        {
            out.push_back({ toX(zone->StartNs), toX(zone->StartNs + zone->DurationNs), zone->Depth, zone->Name,
                            static_cast<double>(zone->DurationNs) / NS_PER_MS, zone });
        }
    }

    double GetFrameMs(const Profiling::FrameRecord& frame)
    {
        return frame.EndNs > frame.StartNs ? static_cast<double>(frame.EndNs - frame.StartNs) / NS_PER_MS : 0.0;
    }

    std::string DescribeGpuEvent(const Renderer::GpuTimelineEvent& command)
    {
        const auto counted = [](uint32_t count, const char* one, const char* many)
        {
            return " " + std::to_string(count) + " " + (count == 1 ? one : many);
        };
        if (command.Kind != Renderer::GpuTimelineEventKind::Command)
            return command.Name;
        std::string text = Renderer::GetGpuCommandKindName(command.Command);
        switch (command.Command)
        {
        case Renderer::GpuCommandKind::Draw:        text += counted(command.Count, "vertex", "vertices"); break;
        case Renderer::GpuCommandKind::DrawIndexed: text += counted(command.Count, "index", "indices"); break;
        case Renderer::GpuCommandKind::Barrier:     text += counted(command.Count, "transition", "transitions"); break;
        case Renderer::GpuCommandKind::CopyBuffer:  text += counted(command.Count, "byte", "bytes"); break;
        default: break;
        }
        if (command.Instances > 1)
            text += " x" + counted(command.Instances, "instance", "instances");
        return text;
    }

    void MakeGpuThread(std::span<const Renderer::GpuTimelineEvent> events, uint64_t frameStartNs, Profiling::ThreadRecord& out)
    {
        out.ThreadIndex = GPU_THREAD_INDEX;
        out.ThreadName  = GPU_THREAD_NAME;
        out.Zones.clear();
        for (const Renderer::GpuTimelineEvent& event : events)
        {
            const char* name = event.Kind == Renderer::GpuTimelineEventKind::Command
                             ? Profiling::InternName(DescribeGpuEvent(event)) : event.Name;
            out.Zones.push_back({ name, frameStartNs + static_cast<uint64_t>(std::llround(event.StartMs * NS_PER_MS)),
                                  static_cast<uint64_t>(std::llround(event.DurationMs * NS_PER_MS)), event.Depth });
        }
    }

    const char* GetZoneCategoryName(ZoneCategory category)
    {
        switch (category)
        {
        case ZoneCategory::Rendering:     return "Rendering";
        case ZoneCategory::Scripts:       return "Scripts";
        case ZoneCategory::Physics:       return "Physics";
        case ZoneCategory::Animation:     return "Animation";
        case ZoneCategory::UI:            return "UI";
        case ZoneCategory::WaitingForGpu: return "Waiting for GPU";
        case ZoneCategory::Others:        return "Others";
        }
        return "?";
    }

    uint32_t GetZoneCategoryColor(ZoneCategory category)
    {
        switch (category)
        {
        case ZoneCategory::Rendering:     return 0x6FB34A;
        case ZoneCategory::Scripts:       return 0x4A90D9;
        case ZoneCategory::Physics:       return 0xE8A23A;
        case ZoneCategory::Animation:     return 0x3FBFB0;
        case ZoneCategory::UI:            return 0xA77BD6;
        case ZoneCategory::WaitingForGpu: return 0xD9C548;
        case ZoneCategory::Others:        return 0x8A8A8A;
        }
        return 0x8A8A8A;
    }

    std::optional<ZoneCategory> CategorizeZone(std::string_view name)
    {
        struct Rule
        {
            std::string_view Text;
            bool             Prefix;
            ZoneCategory     Category;
        };
        static constexpr Rule RULES[] = {
            { "Render", false, ZoneCategory::Rendering },
            { "RenderFrame", false, ZoneCategory::Rendering },
            { "SyncResources", false, ZoneCategory::Rendering },
            { "Extract: ", true, ZoneCategory::Rendering },
            { "Scripts: ", true, ZoneCategory::Scripts },
            { "ScriptSystem", false, ZoneCategory::Scripts },
            { "Physics: ", true, ZoneCategory::Physics },
            { "PhysicsSystem", false, ZoneCategory::Physics },
            { "Phase: Animation", false, ZoneCategory::Animation },
            { "AnimationSystem", false, ZoneCategory::Animation },
            { "Editor UI", false, ZoneCategory::UI },
            { "Outside windows", false, ZoneCategory::UI },
            { "UiSystem", false, ZoneCategory::UI },
            { "Wait for the GPU", false, ZoneCategory::WaitingForGpu },
        };
        for (const Rule& rule : RULES)
        {
            if (rule.Prefix ? name.starts_with(rule.Text) : name == rule.Text)
                return rule.Category;
        }
        return std::nullopt;
    }

    CategoryTimes ComputeCategoryTimes(const Profiling::ThreadRecord& thread)
    {
        CategoryTimes times{};
        // Walking zones in start order with a stack of the open ones and their categories: a zone's
        // whole duration goes to its category and leaves its parent's, which keeps only self time.
        std::vector<std::pair<const Profiling::ZoneRecord*, ZoneCategory>> open;
        for (const Profiling::ZoneRecord* zone : SortZonesByStart(thread))
        {
            while (!open.empty() && open.back().first->Depth >= zone->Depth)
                open.pop_back();
            const ZoneCategory inherited = open.empty() ? ZoneCategory::Others : open.back().second;
            const ZoneCategory category  = CategorizeZone(zone->Name ? zone->Name : "").value_or(inherited);
            const double       ms        = static_cast<double>(zone->DurationNs) / NS_PER_MS;
            times[static_cast<size_t>(category)] += ms;
            if (!open.empty())
                times[static_cast<size_t>(open.back().second)] -= ms;
            open.emplace_back(zone, category);
        }
        return times;
    }

    const char* GetRenderingCounterName(RenderingCounter counter)
    {
        switch (counter)
        {
        case RenderingCounter::DrawCalls: return "Draw calls";
        case RenderingCounter::Commands:  return "Commands";
        case RenderingCounter::Triangles: return "Triangles";
        case RenderingCounter::Vertices:  return "Vertices";
        case RenderingCounter::Barriers:  return "Barriers";
        case RenderingCounter::Copies:    return "Copies";
        }
        return "?";
    }

    RenderingCounters CountRendering(std::span<const Renderer::GpuTimelineEvent> events)
    {
        RenderingCounters counts{};
        const auto add = [&](RenderingCounter counter, uint64_t value) { counts[static_cast<size_t>(counter)] += value; };
        for (const Renderer::GpuTimelineEvent& command : events)
        {
            if (command.Kind != Renderer::GpuTimelineEventKind::Command)
                continue;
            add(RenderingCounter::Commands, 1);
            switch (command.Command)
            {
            case Renderer::GpuCommandKind::Draw:
            case Renderer::GpuCommandKind::DrawIndexed:
            {
                const uint64_t instances = std::max<uint32_t>(command.Instances, 1);
                add(RenderingCounter::DrawCalls, 1);
                add(RenderingCounter::Triangles, command.Count / 3 * instances);
                add(RenderingCounter::Vertices, static_cast<uint64_t>(command.Count) * instances);
                break;
            }
            case Renderer::GpuCommandKind::Barrier:
                add(RenderingCounter::Barriers, 1);
                break;
            case Renderer::GpuCommandKind::CopyBuffer:
            case Renderer::GpuCommandKind::CopyToTexture:
            case Renderer::GpuCommandKind::Mipmaps:
            case Renderer::GpuCommandKind::Blit:
                add(RenderingCounter::Copies, 1);
                break;
            default:
                break;
            }
        }
        return counts;
    }

    std::string FormatCount(uint64_t count)
    {
        char text[32];
        if (count < 10000)
            std::snprintf(text, sizeof(text), "%llu", static_cast<unsigned long long>(count));
        else if (count < 1000000)
            std::snprintf(text, sizeof(text), "%.1fk", static_cast<double>(count) / 1.0e3);
        else
            std::snprintf(text, sizeof(text), "%.1fM", static_cast<double>(count) / 1.0e6);
        return text;
    }
}

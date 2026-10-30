#include "ProfilerAnalysis.hpp"

#include <algorithm>
#include <cstring>

namespace Editor
{
    namespace
    {
        constexpr double NS_PER_MS = 1.0e6;

        // The thread's zones by start, a parent before the zones nested in it.
        std::vector<const Profiling::ZoneRecord*> SortByStart(const Profiling::ThreadRecord& thread)
        {
            std::vector<const Profiling::ZoneRecord*> zones;
            zones.reserve(thread.Zones.size());
            for (const Profiling::ZoneRecord& zone : thread.Zones)
                zones.push_back(&zone);
            std::sort(zones.begin(), zones.end(), [](const Profiling::ZoneRecord* a, const Profiling::ZoneRecord* b)
            {
                return a->StartNs != b->StartNs ? a->StartNs < b->StartNs : a->Depth < b->Depth;
            });
            return zones;
        }
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
            for (const Profiling::ZoneRecord* zone : SortByStart(thread))
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
        for (const Profiling::ZoneRecord* zone : SortByStart(thread))
        {
            out.push_back({ toX(zone->StartNs), toX(zone->StartNs + zone->DurationNs), zone->Depth, zone->Name,
                            static_cast<double>(zone->DurationNs) / NS_PER_MS });
        }
    }

    double GetFrameMs(const Profiling::FrameRecord& frame)
    {
        return frame.EndNs > frame.StartNs ? static_cast<double>(frame.EndNs - frame.StartNs) / NS_PER_MS : 0.0;
    }

    std::string DescribeGpuCommand(const Renderer::GpuCommandTiming& command)
    {
        const auto counted = [](uint32_t count, const char* one, const char* many)
        {
            return " " + std::to_string(count) + " " + (count == 1 ? one : many);
        };
        std::string text = Renderer::GetGpuCommandKindName(command.Kind);
        switch (command.Kind)
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

    void SummarizeCommandsByPass(std::span<const Renderer::GpuCommandTiming> commands, std::vector<PassCommandStat>& out)
    {
        out.clear();
        for (const Renderer::GpuCommandTiming& command : commands)
        {
            auto stat = std::find_if(out.begin(), out.end(),
                                     [&](const PassCommandStat& s) { return std::strcmp(s.Pass.c_str(), command.Pass) == 0; });
            if (stat == out.end())
            {
                out.push_back({ command.Pass });
                stat = out.end() - 1;
            }
            ++stat->Commands;
            if (command.Kind == Renderer::GpuCommandKind::Draw || command.Kind == Renderer::GpuCommandKind::DrawIndexed)
                ++stat->Draws;
            stat->GpuMs += command.GpuMilliseconds;
        }
    }
}

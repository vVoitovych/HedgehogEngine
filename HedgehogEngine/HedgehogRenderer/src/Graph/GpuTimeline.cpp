#include "HedgehogRenderer/Graph/GpuTimeline.hpp"

#include "Profiler/api/Profiler.hpp"

#include <algorithm>

namespace Renderer
{
    namespace
    {
        // Scopes nest a few deep; ones past this are left out, their ends still matched.
        constexpr size_t MAX_SCOPE_DEPTH = 16;
    }

    void BuildGpuTimeline(uint64_t frameStart, std::span<const std::string> passNames, std::span<const uint64_t> passTicks,
                          std::span<const GpuCommandTiming> commands, std::span<const uint64_t> commandTicks,
                          double msPerTick, std::vector<GpuTimelineEvent>& out)
    {
        out.clear();
        if (passNames.empty() || passTicks.size() < passNames.size() + 1)
            return;
        const bool timed = !commands.empty() && commandTicks.size() >= commands.size() + 1;
        const auto ms    = [&](uint64_t tick) { return tick > frameStart ? static_cast<double>(tick - frameStart) * msPerTick : 0.0; };

        size_t c = 0;
        for (size_t p = 0; p < passNames.size(); ++p)
        {
            const char*  pass      = Profiling::InternName(passNames[p]);
            const size_t passEvent = out.size();
            const double passEnd   = ms(passTicks[p + 1]);
            GpuTimelineEvent event;
            event.Name       = pass;
            event.Pass       = pass;
            event.StartMs    = ms(passTicks[p]);
            event.DurationMs = std::max(passEnd - event.StartMs, 0.0);
            out.push_back(event);

            // The open scopes' events; skipped counts those opened past MAX_SCOPE_DEPTH.
            size_t open[MAX_SCOPE_DEPTH];
            size_t depth   = 0;
            size_t skipped = 0;
            for (; timed && c < commands.size() && commandTicks[c + 1] <= passTicks[p + 1]; ++c)
            {
                const GpuCommandTiming& command = commands[c];
                const double            end     = ms(commandTicks[c + 1]);
                if (command.Kind == GpuCommandKind::ScopeEnd)
                {
                    if (skipped > 0)
                        --skipped;
                    else if (depth > 0)
                    {
                        GpuTimelineEvent& scope = out[open[--depth]];
                        scope.DurationMs        = std::max(end - scope.StartMs, 0.0);
                    }
                    continue;
                }
                const double floor = out[depth > 0 ? open[depth - 1] : passEvent].StartMs;
                GpuTimelineEvent item;
                item.Pass  = pass;
                item.Depth = static_cast<uint32_t>(depth + 1);
                if (command.Kind == GpuCommandKind::ScopeBegin)
                {
                    if (depth == MAX_SCOPE_DEPTH)
                    {
                        ++skipped;
                        continue;
                    }
                    // A scope starts at its own timestamp.
                    item.Kind    = GpuTimelineEventKind::Scope;
                    item.Name    = command.Scope != nullptr ? command.Scope : "Scope";
                    item.StartMs = std::max(end, floor);
                    open[depth++] = out.size();
                    out.push_back(item);
                    continue;
                }
                item.Kind       = GpuTimelineEventKind::Command;
                item.Name       = GetGpuCommandKindName(command.Kind);
                item.Command    = command.Kind;
                item.Count      = command.Count;
                item.Instances  = command.Instances;
                item.StartMs    = std::max(ms(commandTicks[c]), floor);
                item.DurationMs = std::max(end - item.StartMs, 0.0);
                out.push_back(item);
            }
            while (depth > 0)
            {
                GpuTimelineEvent& scope = out[open[--depth]];
                scope.DurationMs        = std::max(passEnd - scope.StartMs, 0.0);
            }
        }
    }
}

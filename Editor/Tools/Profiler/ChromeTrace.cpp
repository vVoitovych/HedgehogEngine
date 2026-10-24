#include "ChromeTrace.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <set>
#include <utility>

namespace Editor
{
    namespace
    {
        constexpr int CPU_PROCESS = 1;
        constexpr int GPU_PROCESS = 2;

        void AppendEscaped(std::string& out, std::string_view text)
        {
            for (const char c : text)
            {
                switch (c)
                {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20)
                    {
                        char code[8];
                        std::snprintf(code, sizeof(code), "\\u%04x", static_cast<unsigned>(c));
                        out += code;
                    }
                    else
                        out += c;
                }
            }
        }

        void AppendMicroseconds(std::string& out, double ns)
        {
            char number[32];
            std::snprintf(number, sizeof(number), "%.3f", ns / 1000.0);
            out += number;
        }

        // Events separated by a comma and a line break, so a file reads one event per line.
        struct TraceWriter
        {
            std::string Out   = "{\"displayTimeUnit\":\"ms\",\"traceEvents\":[";
            bool        First = true;

            void Next()
            {
                if (!First)
                    Out += ",\n";
                First = false;
            }

            void Metadata(int pid, int tid, const char* what, std::string_view name)
            {
                Next();
                Out += "{\"name\":\"";
                Out += what;
                Out += "\",\"ph\":\"M\",\"pid\":" + std::to_string(pid) + ",\"tid\":" + std::to_string(tid) +
                       ",\"args\":{\"name\":\"";
                AppendEscaped(Out, name);
                Out += "\"}}";
            }

            void Complete(int pid, int tid, std::string_view name, double startNs, double durationNs)
            {
                Next();
                Out += "{\"name\":\"";
                AppendEscaped(Out, name);
                Out += "\",\"ph\":\"X\",\"pid\":" + std::to_string(pid) + ",\"tid\":" + std::to_string(tid) + ",\"ts\":";
                AppendMicroseconds(Out, startNs);
                Out += ",\"dur\":";
                AppendMicroseconds(Out, durationNs);
                Out += "}";
            }
        };
    }

    std::string WriteChromeTrace(std::span<const Profiling::FrameRecord* const> frames,
                                 std::span<const TraceGpuFrame>                gpuFrames)
    {
        // Every time is from the earliest one, a zone begun before its frame included, so the
        // numbers stay small.
        uint64_t origin = std::numeric_limits<uint64_t>::max();
        for (const Profiling::FrameRecord* frame : frames)
        {
            origin = std::min(origin, frame->StartNs);
            for (const Profiling::ThreadRecord& thread : frame->Threads)
            {
                for (const Profiling::ZoneRecord& zone : thread.Zones)
                    origin = std::min(origin, zone.StartNs);
            }
        }
        for (const TraceGpuFrame& gpu : gpuFrames)
            origin = std::min(origin, gpu.StartNs);
        if (origin == std::numeric_limits<uint64_t>::max())
            origin = 0;
        const auto since = [origin](uint64_t ns) { return static_cast<double>(ns - origin); };

        TraceWriter writer;
        writer.Metadata(CPU_PROCESS, 0, "process_name", "CPU");
        std::set<std::pair<uint32_t, std::string>> named; // each thread named once
        for (const Profiling::FrameRecord* frame : frames)
        {
            for (const Profiling::ThreadRecord& thread : frame->Threads)
            {
                if (named.emplace(thread.ThreadIndex, thread.ThreadName).second)
                    writer.Metadata(CPU_PROCESS, static_cast<int>(thread.ThreadIndex), "thread_name", thread.ThreadName);
            }
        }
        for (const Profiling::FrameRecord* frame : frames)
        {
            for (const Profiling::ThreadRecord& thread : frame->Threads)
            {
                for (const Profiling::ZoneRecord& zone : thread.Zones)
                    writer.Complete(CPU_PROCESS, static_cast<int>(thread.ThreadIndex), zone.Name, since(zone.StartNs),
                                    static_cast<double>(zone.DurationNs));
            }
        }

        if (!gpuFrames.empty())
        {
            writer.Metadata(GPU_PROCESS, 0, "process_name", "GPU");
            writer.Metadata(GPU_PROCESS, 0, "thread_name", "Render graph passes");
        }
        for (const TraceGpuFrame& gpu : gpuFrames)
        {
            double start = since(gpu.StartNs);
            for (const Renderer::GpuPassTiming& pass : gpu.Passes)
            {
                const double duration = pass.GpuMilliseconds * 1.0e6;
                writer.Complete(GPU_PROCESS, 0, pass.Name, start, duration);
                start += duration;
            }
        }

        writer.Out += "]}\n";
        return writer.Out;
    }
}

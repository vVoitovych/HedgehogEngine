#include "FrameStats.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <cstdio>
#include <numeric>

namespace Renderer
{
    void FrameStats::BeginCapture()
    {
        m_Zones.clear();
        m_Capturing = true;
    }

    void FrameStats::EndCapture()
    {
        m_Capturing = false;
    }

    void FrameStats::AddSample(const char* zoneName, double milliseconds)
    {
        if (!m_Capturing)
            return;

        findOrAddZone(zoneName).SamplesMs.push_back(milliseconds);
    }

    void FrameStats::AddGpuSample(const char* zoneName, double milliseconds)
    {
        if (!m_Capturing)
            return;

        findOrAddZone(zoneName).GpuSamplesMs.push_back(milliseconds);
    }

    void FrameStats::LogReport() const
    {
        if (m_Zones.empty())
        {
            LOGWARNING("FrameStats: no samples captured.");
            return;
        }

        LOGINFO("FrameStats | zone                 |   avg ms |   min ms |   max ms |   p95 ms | samples |  gpu avg ms");

        for (const auto& zone : m_Zones)
        {
            char gpu[32] = "           -";
            if (!zone.GpuSamplesMs.empty())
            {
                const double gpuAvg = std::accumulate(zone.GpuSamplesMs.begin(), zone.GpuSamplesMs.end(), 0.0)
                                    / static_cast<double>(zone.GpuSamplesMs.size());
                std::snprintf(gpu, sizeof(gpu), "%12.3f", gpuAvg);
            }

            if (zone.SamplesMs.empty())
            {
                char line[192];
                std::snprintf(line, sizeof(line),
                    "FrameStats | %-20s | %8s | %8s | %8s | %8s | %7s | %s", zone.Name.c_str(), "-", "-", "-", "-", "-", gpu);
                LOGINFO(line);
                continue;
            }

            std::vector<double> sorted = zone.SamplesMs;
            std::sort(sorted.begin(), sorted.end());

            const double avg = std::accumulate(sorted.begin(), sorted.end(), 0.0)
                             / static_cast<double>(sorted.size());
            const double min = sorted.front();
            const double max = sorted.back();
            const size_t p95Index = std::min(sorted.size() - 1,
                static_cast<size_t>(static_cast<double>(sorted.size()) * 0.95));
            const double p95 = sorted[p95Index];

            char line[192];
            std::snprintf(line, sizeof(line),
                "FrameStats | %-20s | %8.3f | %8.3f | %8.3f | %8.3f | %7zu | %s",
                zone.Name.c_str(), avg, min, max, p95, sorted.size(), gpu);
            LOGINFO(line);
        }
    }

    FrameStats::ZoneSeries& FrameStats::findOrAddZone(const char* zoneName)
    {
        for (auto& zone : m_Zones)
        {
            if (zone.Name == zoneName)
                return zone;
        }

        m_Zones.push_back({ zoneName, {} });
        return m_Zones.back();
    }
}

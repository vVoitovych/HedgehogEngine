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

        findOrAddZone(zoneName).m_SamplesMs.push_back(milliseconds);
    }

    void FrameStats::LogReport() const
    {
        if (m_Zones.empty())
        {
            LOGWARNING("FrameStats: no samples captured.");
            return;
        }

        LOGINFO("FrameStats | zone                 |   avg ms |   min ms |   max ms |   p95 ms | samples");

        for (const auto& zone : m_Zones)
        {
            if (zone.m_SamplesMs.empty())
                continue;

            std::vector<double> sorted = zone.m_SamplesMs;
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
                "FrameStats | %-20s | %8.3f | %8.3f | %8.3f | %8.3f | %7zu",
                zone.m_Name.c_str(), avg, min, max, p95, sorted.size());
            LOGINFO(line);
        }
    }

    FrameStats::ZoneSeries& FrameStats::findOrAddZone(const char* zoneName)
    {
        for (auto& zone : m_Zones)
        {
            if (zone.m_Name == zoneName)
                return zone;
        }

        m_Zones.push_back({ zoneName, {} });
        return m_Zones.back();
    }
}
